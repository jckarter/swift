//===--- ProtocolConformanceRef.cpp - AST Protocol Conformance Reference --===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2022 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file implements the ProtocolConformanceRef structure, which wraps a
// concrete or abstract conformance, or is invalid.
//
//===----------------------------------------------------------------------===//

#include "swift/AST/ProtocolConformanceRef.h"
#include "AbstractConformance.h"
#include "swift/AST/ASTContext.h"
#include "swift/AST/ConformanceLookup.h"
#include "swift/AST/Decl.h"
#include "swift/AST/GenericEnvironment.h"
#include "swift/AST/InFlightSubstitution.h"
#include "swift/AST/Module.h"
#include "swift/AST/PackConformance.h"
#include "swift/AST/ProtocolConformance.h"
#include "swift/AST/TypeCheckRequests.h"
#include "swift/AST/Types.h"
#include "swift/Basic/Assertions.h"

#define DEBUG_TYPE "AST"

using namespace swift;

bool ProtocolConformanceRef::isInvalid() const {
  if (!Union)
    return true;

  if (auto pack = Union.dyn_cast<PackConformance *>())
    return pack->isInvalid();

  return false;
}

Type ProtocolConformanceRef::getType() const {
  if (isInvalid())
    return Type();

  if (isConcrete())
    return getConcrete()->getType();

  if (isPack())
    return Type(getPack()->getType());

  return getAbstract()->getType();
}

ProtocolDecl *ProtocolConformanceRef::getProtocol() const {
  if (isConcrete()) {
    return getConcrete()->getProtocol();
  } else if (isPack()) {
    return getPack()->getProtocol();
  } else {
    return getAbstract()->getProtocol();
  }
}

ProtocolConformanceRef
ProtocolConformanceRef::subst(Type origType,
                              SubstitutionMap subMap,
                              SubstOptions options) const {
  InFlightSubstitutionViaSubMap IFS(subMap, options);
  return subst(origType, IFS);
}

ProtocolConformanceRef
ProtocolConformanceRef::subst(Type origType,
                              TypeSubstitutionFn subs,
                              LookupConformanceFn conformances,
                              SubstOptions options) const {
  InFlightSubstitution IFS(subs, conformances, options);
  return subst(origType, IFS);
}

ProtocolConformanceRef
ProtocolConformanceRef::subst(Type origType, InFlightSubstitution &IFS) const {
  if (isInvalid())
    return *this;

  if (isConcrete())
    return getConcrete()->subst(IFS);
  if (isPack())
    return getPack()->subst(IFS);

  ASSERT(isAbstract());
  auto *proto = getProtocol();

  // If the type is an opaque archetype, the conformance will remain abstract,
  // unless we're specifically substituting opaque types.
  if (origType->getAs<OpaqueTypeArchetypeType>()) {
    if (!IFS.shouldSubstituteOpaqueArchetypes()) {
      return forAbstract(origType.subst(IFS), proto);
    }
  }

  // FIXME: Handle local archetypes as above!

  // Otherwise, compute the substituted type.
  auto substType = origType.subst(IFS);

  // If the type is an existential, it must be self-conforming.
  // FIXME: This feels like it's in the wrong place.
  if (substType->isExistentialType()) {
    auto optConformance =
        lookupConformance(substType, proto, /*allowMissing=*/true);
    if (optConformance)
      return optConformance;

    return ProtocolConformanceRef::forInvalid();
  }

  // Local conformance lookup into the substitution map.
  // FIXME: Pack element level?
  return IFS.lookupConformance(origType->getCanonicalType(), substType, proto,
                               /*level=*/0);
}

ProtocolConformanceRef ProtocolConformanceRef::mapConformanceOutOfContext() const {
  if (isConcrete()) {
    return getConcrete()->subst(
        MapTypeOutOfContext(),
        MakeAbstractConformanceForGenericType(),
        SubstFlags::PreservePackExpansionLevel |
        SubstFlags::SubstitutePrimaryArchetypes);
  } else if (isPack()) {
    return getPack()->subst(
        MapTypeOutOfContext(),
        MakeAbstractConformanceForGenericType(),
        SubstFlags::PreservePackExpansionLevel |
        SubstFlags::SubstitutePrimaryArchetypes);
  } else if (isAbstract()) {
    auto *abstract = getAbstract();
    return forAbstract(abstract->getType()->mapTypeOutOfContext(),
                       abstract->getProtocol());
  }

  return *this;
}

Type
ProtocolConformanceRef::getTypeWitnessByName(Type type, Identifier name) const {
  assert(!isInvalid());

  // Find the named requirement.
  ProtocolDecl *proto = getProtocol();
  auto *assocType = proto->getAssociatedType(name);

  // FIXME: Shouldn't this be a hard error?
  if (!assocType)
    return ErrorType::get(proto->getASTContext());

  return getTypeWitness(type, assocType);
}

ConcreteDeclRef
ProtocolConformanceRef::getWitnessByName(Type type, DeclName name) const {
  // Find the named requirement.
  auto *proto = getProtocol();
  auto *requirement = proto->getSingleRequirement(name);
  if (requirement == nullptr)
    return ConcreteDeclRef();

  // For a type with dependent conformance, just return the requirement from
  // the protocol. There are no protocol conformance tables.
  if (!isConcrete()) {
    auto subs = SubstitutionMap::getProtocolSubstitutions(proto, type, *this);
    return ConcreteDeclRef(requirement, subs);
  }

  return getConcrete()->getWitnessDeclRef(requirement);
}

ArrayRef<Requirement>
ProtocolConformanceRef::getConditionalRequirements() const {
  if (isConcrete())
    return getConcrete()->getConditionalRequirements();
  else
    // An abstract conformance is never conditional, as above.
    return {};
}

Type ProtocolConformanceRef::getTypeWitness(Type conformingType,
                                            AssociatedTypeDecl *assocType,
                                            SubstOptions options) const {
  if (isPack()) {
    auto *pack = getPack();
    ASSERT(conformingType->isEqual(pack->getType()));
    return pack->getTypeWitness(assocType);
  }

  auto failed = [&]() {
    return DependentMemberType::get(ErrorType::get(conformingType),
                                    assocType);
  };

  if (isInvalid())
    return failed();

  auto proto = getProtocol();
  ASSERT(assocType->getProtocol() == proto);

  if (isConcrete()) {
    auto witnessType = getConcrete()->getTypeWitness(assocType, options);
    if (!witnessType || witnessType->is<ErrorType>())
      return failed();
    return witnessType;
  }

  ASSERT(isAbstract());

  if (auto *archetypeType = conformingType->getAs<ArchetypeType>()) {
    return archetypeType->getNestedType(assocType);
  }

  CONDITIONAL_ASSERT(conformingType->isTypeParameter() ||
                     conformingType->isTypeVariableOrMember() ||
                     conformingType->is<UnresolvedType>() ||
                     conformingType->is<PlaceholderType>());

  return DependentMemberType::get(conformingType, assocType);
}

Type ProtocolConformanceRef::getAssociatedType(Type conformingType,
                                               Type assocType) const {
  if (isInvalid())
    return ErrorType::get(assocType->getASTContext());

  auto proto = getProtocol();

  auto substMap =
    SubstitutionMap::getProtocolSubstitutions(proto, conformingType, *this);
  return assocType.subst(substMap);
}

ProtocolConformanceRef
ProtocolConformanceRef::getAssociatedConformance(Type conformingType,
                                                 Type assocType,
                                                 ProtocolDecl *protocol) const {
  // If this is a pack conformance, project the associated conformances from
  // each pack element.
  if (isPack()) {
    auto *pack = getPack();
    assert(conformingType->isEqual(pack->getType()));
    return ProtocolConformanceRef(
        pack->getAssociatedConformance(assocType, protocol));
  }

  // If this is a concrete conformance, project the associated conformance.
  if (isConcrete()) {
    auto conformance = getConcrete();
    assert(conformance->getType()->isEqual(conformingType));
    return conformance->getAssociatedConformance(assocType, protocol);
  }

  auto computeSubjectType = [&](Type conformingType) -> Type {
    return assocType.transformRec(
      [&](TypeBase *t) -> std::optional<Type> {
        if (isa<GenericTypeParamType>(t))
          return conformingType;
        return std::nullopt;
      });
  };

  // An associated conformance of an archetype might be known to be
  // a concrete conformance, if the subject type is fixed to a concrete
  // type in the archetype's generic signature. We don't actually have
  // any way to recover the conformance in this case, except via global
  // conformance lookup.
  //
  // However, if we move to a first-class representation of abstract
  // conformances where they store their subject types, we can also
  // cache the lookups inside the abstract conformance instance too.
  if (auto archetypeType = conformingType->getAs<ArchetypeType>()) {
    auto *genericEnv = archetypeType->getGenericEnvironment();
    auto subjectType = computeSubjectType(archetypeType->getInterfaceType());

    return lookupConformance(
        genericEnv->mapTypeIntoContext(subjectType),
        protocol);
  }

  // Associated conformances of type parameters and type variables
  // are always abstract, because we don't know the output generic
  // signature of the substitution (or in the case of type variables,
  // we have no visibility into constraints). See the parallel hack
  // to handle this in SubstitutionMap::lookupConformance().
  auto subjectType = computeSubjectType(conformingType);
  return ProtocolConformanceRef::forAbstract(subjectType, protocol);
}

/// Check of all types used by the conformance are canonical.
bool ProtocolConformanceRef::isCanonical() const {
  if (isInvalid())
    return true;

  if (isPack())
    return getPack()->isCanonical();

  if (isAbstract())
    return getType()->isCanonical();

  return getConcrete()->isCanonical();
}

ProtocolConformanceRef
ProtocolConformanceRef::getCanonicalConformanceRef() const {
  if (isInvalid())
    return *this;

  if (isPack())
    return ProtocolConformanceRef(getPack()->getCanonicalConformance());

  if (isAbstract())
    return forAbstract(getType()->getCanonicalType(), getProtocol());

  return ProtocolConformanceRef(getConcrete()->getCanonicalConformance());
}

bool ProtocolConformanceRef::hasUnavailableConformance() const {
  if (isInvalid() || isAbstract())
    return false;

  if (isPack()) {
    for (auto conformance : getPack()->getPatternConformances()) {
      if (conformance.hasUnavailableConformance())
        return true;
    }

    return false;
  }

  // Check whether this conformance is on an unavailable extension.
  auto concrete = getConcrete();
  auto *dc = concrete->getRootConformance()->getDeclContext();
  auto ext = dyn_cast<ExtensionDecl>(dc);
  if (ext && ext->isUnavailable())
    return true;

  // Check the conformances in the substitution map.
  auto subMap = concrete->getSubstitutionMap();
  for (auto subConformance : subMap.getConformances()) {
    if (subConformance.hasUnavailableConformance())
      return true;
  }

  return false;
}

bool ProtocolConformanceRef::hasMissingConformance() const {
  return forEachMissingConformance(
      [](BuiltinProtocolConformance *builtin) {
        return true;
      });
}

bool ProtocolConformanceRef::forEachMissingConformance(
    llvm::function_ref<bool(BuiltinProtocolConformance *missing)> fn) const {
  if (isInvalid() || isAbstract())
    return false;

  if (isPack()) {
    for (auto conformance : getPack()->getPatternConformances()) {
      if (conformance.forEachMissingConformance(fn))
        return true;
    }

    return false;
  }

  // Is this a missing conformance?
  ProtocolConformance *concreteConf = getConcrete();
  RootProtocolConformance *rootConf = concreteConf->getRootConformance();
  if (auto builtinConformance = dyn_cast<BuiltinProtocolConformance>(rootConf)){
    if (builtinConformance->isMissing() && fn(builtinConformance))
      return true;
  }

  // Check conformances that are part of this conformance.
  auto subMap = concreteConf->getSubstitutionMap();
  for (auto conformance : subMap.getConformances()) {
    if (conformance.forEachMissingConformance(fn))
      return true;
  }

  return false;
}

bool ProtocolConformanceRef::forEachIsolatedConformance(
    llvm::function_ref<bool(ProtocolConformanceRef)> body
) const {
  if (isInvalid() || isAbstract())
    return false;

  if (isPack()) {
    auto pack = getPack()->getPatternConformances();
    for (auto conformance : pack) {
      if (conformance.forEachIsolatedConformance(body))
        return true;
    }

    return false;
  }

  // Is this an isolated conformance?
  auto concrete = getConcrete();
  if (auto normal =
          dyn_cast<NormalProtocolConformance>(concrete->getRootConformance())) {
    if (normal->isIsolated()) {
      if (body(*this))
        return true;
    }
  }

  // Check conformances that are part of this conformance.
  auto subMap = concrete->getSubstitutionMap();
  for (auto conformance : subMap.getConformances()) {
    if (conformance.forEachIsolatedConformance(body))
      return true;
  }

  return false;
}

void swift::simple_display(llvm::raw_ostream &out, ProtocolConformanceRef conformanceRef) {
  if (conformanceRef.isAbstract()) {
    simple_display(out, conformanceRef.getProtocol());
  } else if (conformanceRef.isConcrete()) {
    simple_display(out, conformanceRef.getConcrete());
  } else if (conformanceRef.isPack()) {
    simple_display(out, conformanceRef.getPack());
  }
}

SourceLoc swift::extractNearestSourceLoc(const ProtocolConformanceRef conformanceRef) {
  if (conformanceRef.isAbstract()) {
    return extractNearestSourceLoc(conformanceRef.getProtocol());
  } else if (conformanceRef.isConcrete()) {
    return extractNearestSourceLoc(conformanceRef.getConcrete());
  }
  return SourceLoc();
}
