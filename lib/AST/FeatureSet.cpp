//===--- FeatureSet.cpp - Language feature support --------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2024 - 2025 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "FeatureSet.h"

#include "swift/AST/Decl.h"
#include "swift/AST/ExistentialLayout.h"
#include "swift/AST/GenericParamList.h"
#include "swift/AST/NameLookup.h"
#include "swift/AST/ParameterList.h"
#include "swift/AST/Pattern.h"
#include "swift/AST/ProtocolConformance.h"
#include "clang/AST/DeclObjC.h"
#include "swift/Basic/Assertions.h"

using namespace swift;

/// Does the interface of this declaration use a type for which the
/// given predicate returns true?
static bool usesTypeMatching(const Decl *decl,
                             llvm::function_ref<bool(Type)> fn) {
  if (auto value = dyn_cast<ValueDecl>(decl)) {
    if (Type type = value->getInterfaceType()) {
      return type.findIf(fn);
    }
  }

  return false;
}

// ----------------------------------------------------------------------------
// MARK: - Standard Features
// ----------------------------------------------------------------------------

/// Functions to determine which features a particular declaration uses. The
/// usesFeatureNNN functions correspond to the features in Features.def.

#define BASELINE_LANGUAGE_FEATURE(FeatureName, SENumber, Description)          \
  static bool usesFeature##FeatureName(Decl *decl) { return false; }
#define LANGUAGE_FEATURE(FeatureName, SENumber, Description)
#include "swift/Basic/Features.def"

#define UNINTERESTING_FEATURE(FeatureName)                                     \
  static bool usesFeature##FeatureName(Decl *decl) { return false; }

// ----------------------------------------------------------------------------
// MARK: - Upcoming Features
// ----------------------------------------------------------------------------

UNINTERESTING_FEATURE(ConciseMagicFile)
UNINTERESTING_FEATURE(ForwardTrailingClosures)
UNINTERESTING_FEATURE(StrictConcurrency)
UNINTERESTING_FEATURE(BareSlashRegexLiterals)
UNINTERESTING_FEATURE(DeprecateApplicationMain)
UNINTERESTING_FEATURE(ImportObjcForwardDeclarations)
UNINTERESTING_FEATURE(DisableOutwardActorInference)
UNINTERESTING_FEATURE(InternalImportsByDefault)
UNINTERESTING_FEATURE(IsolatedDefaultValues)
UNINTERESTING_FEATURE(GlobalConcurrency)
UNINTERESTING_FEATURE(FullTypedThrows)
UNINTERESTING_FEATURE(ExistentialAny)
UNINTERESTING_FEATURE(InferSendableFromCaptures)
UNINTERESTING_FEATURE(ImplicitOpenExistentials)
UNINTERESTING_FEATURE(MemberImportVisibility)

// ----------------------------------------------------------------------------
// MARK: - Experimental Features
// ----------------------------------------------------------------------------

UNINTERESTING_FEATURE(StaticAssert)
UNINTERESTING_FEATURE(NamedOpaqueTypes)
UNINTERESTING_FEATURE(FlowSensitiveConcurrencyCaptures)
UNINTERESTING_FEATURE(CodeItemMacros)
UNINTERESTING_FEATURE(PreambleMacros)
UNINTERESTING_FEATURE(TupleConformances)
UNINTERESTING_FEATURE(SymbolLinkageMarkers)
UNINTERESTING_FEATURE(LazyImmediate)
UNINTERESTING_FEATURE(MoveOnlyClasses)
UNINTERESTING_FEATURE(NoImplicitCopy)
UNINTERESTING_FEATURE(OldOwnershipOperatorSpellings)
UNINTERESTING_FEATURE(MoveOnlyEnumDeinits)
UNINTERESTING_FEATURE(MoveOnlyTuples)
UNINTERESTING_FEATURE(MoveOnlyPartialReinitialization)
UNINTERESTING_FEATURE(LayoutPrespecialization)
UNINTERESTING_FEATURE(AccessLevelOnImport)
UNINTERESTING_FEATURE(AllowNonResilientAccessInPackage)
UNINTERESTING_FEATURE(ClientBypassResilientAccessInPackage)
UNINTERESTING_FEATURE(LayoutStringValueWitnesses)
UNINTERESTING_FEATURE(LayoutStringValueWitnessesInstantiation)
UNINTERESTING_FEATURE(DifferentiableProgramming)
UNINTERESTING_FEATURE(ForwardModeDifferentiation)
UNINTERESTING_FEATURE(AdditiveArithmeticDerivedConformances)
UNINTERESTING_FEATURE(SendableCompletionHandlers)
UNINTERESTING_FEATURE(OpaqueTypeErasure)
UNINTERESTING_FEATURE(PackageCMO)
UNINTERESTING_FEATURE(ParserRoundTrip)
UNINTERESTING_FEATURE(ParserValidation)
UNINTERESTING_FEATURE(UnqualifiedLookupValidation)
UNINTERESTING_FEATURE(ImplicitSome)
UNINTERESTING_FEATURE(ParserASTGen)
UNINTERESTING_FEATURE(BuiltinMacros)
UNINTERESTING_FEATURE(ImportSymbolicCXXDecls)
UNINTERESTING_FEATURE(GenerateBindingsForThrowingFunctionsInCXX)
UNINTERESTING_FEATURE(ReferenceBindings)
UNINTERESTING_FEATURE(BuiltinModule)
UNINTERESTING_FEATURE(RegionBasedIsolation)
UNINTERESTING_FEATURE(PlaygroundExtendedCallbacks)
UNINTERESTING_FEATURE(ThenStatements)
UNINTERESTING_FEATURE(DoExpressions)
UNINTERESTING_FEATURE(ImplicitLastExprResults)
UNINTERESTING_FEATURE(RawLayout)
UNINTERESTING_FEATURE(Embedded)
UNINTERESTING_FEATURE(Volatile)
UNINTERESTING_FEATURE(SuppressedAssociatedTypes)
UNINTERESTING_FEATURE(StructLetDestructuring)
UNINTERESTING_FEATURE(MacrosOnImports)
UNINTERESTING_FEATURE(AsyncCallerExecution)
UNINTERESTING_FEATURE(ExtensibleEnums)
UNINTERESTING_FEATURE(KeyPathWithMethodMembers)

static bool usesFeatureNonescapableTypes(Decl *decl) {
  auto containsNonEscapable =
      [](SmallVectorImpl<InverseRequirement> &inverseReqs) {
        auto foundIt =
            llvm::find_if(inverseReqs, [](InverseRequirement inverseReq) {
              if (inverseReq.getKind() == InvertibleProtocolKind::Escapable) {
                return true;
              }
              return false;
            });
        return foundIt != inverseReqs.end();
      };

  if (auto *valueDecl = dyn_cast<ValueDecl>(decl)) {
    if (isa<StructDecl, EnumDecl, ClassDecl>(decl)) {
      auto *nominalDecl = cast<NominalTypeDecl>(valueDecl);
      InvertibleProtocolSet inverses;
      bool anyObject = false;
      getDirectlyInheritedNominalTypeDecls(nominalDecl, inverses, anyObject);
      if (inverses.containsEscapable()) {
        return true;
      }
    }

    if (auto proto = dyn_cast<ProtocolDecl>(decl)) {
      auto reqSig = proto->getRequirementSignature();

      SmallVector<Requirement, 2> reqs;
      SmallVector<InverseRequirement, 2> inverses;
      reqSig.getRequirementsWithInverses(proto, reqs, inverses);
      if (containsNonEscapable(inverses))
        return true;
    }

    if (isa<AbstractFunctionDecl>(valueDecl) ||
        isa<AbstractStorageDecl>(valueDecl)) {
      if (valueDecl->getInterfaceType().findIf([&](Type type) -> bool {
            if (auto *nominalDecl = type->getAnyNominal()) {
              if (isa<StructDecl, EnumDecl, ClassDecl>(nominalDecl))
                return usesFeatureNonescapableTypes(nominalDecl);
            }
            return false;
          })) {
        return true;
      }
    }
  }

  if (auto *ext = dyn_cast<ExtensionDecl>(decl)) {
    if (auto *nominal = ext->getExtendedNominal())
      if (usesFeatureNonescapableTypes(nominal))
        return true;
  }

  if (auto *genCtx = decl->getAsGenericContext()) {
    if (auto genericSig = genCtx->getGenericSignature()) {
      SmallVector<Requirement, 2> reqs;
      SmallVector<InverseRequirement, 2> inverseReqs;
      genericSig->getRequirementsWithInverses(reqs, inverseReqs);
      if (containsNonEscapable(inverseReqs)) {
        return true;
      }
    }
  }

  return false;
}

static bool usesFeatureInlineArrayTypeSugar(Decl *D) {
  return usesTypeMatching(D, [&](Type ty) {
    return isa<InlineArrayType>(ty.getPointer());
  });
}

UNINTERESTING_FEATURE(StaticExclusiveOnly)
UNINTERESTING_FEATURE(ExtractConstantsFromMembers)
UNINTERESTING_FEATURE(GroupActorErrors)
UNINTERESTING_FEATURE(SameElementRequirements)

static bool usesFeatureSendingArgsAndResults(Decl *decl) {
  auto isFunctionTypeWithSending = [](Type type) {
      auto fnType = type->getAs<AnyFunctionType>();
      if (!fnType)
        return false;

      if (fnType->hasExtInfo() && fnType->hasSendingResult())
        return true;

      return llvm::any_of(fnType->getParams(),
                          [](AnyFunctionType::Param param) {
                            return param.getParameterFlags().isSending();
                          });
  };
  auto declUsesFunctionTypesThatUseSending = [&](Decl *decl) {
    return usesTypeMatching(decl, isFunctionTypeWithSending);
  };

  if (auto *pd = dyn_cast<ParamDecl>(decl)) {
    if (pd->isSending()) {
      return true;
    }

    if (declUsesFunctionTypesThatUseSending(pd))
      return true;
  }

  if (auto *fDecl = dyn_cast<AbstractFunctionDecl>(decl)) {
    // First check for param decl results.
    if (llvm::any_of(fDecl->getParameters()->getArray(), [](ParamDecl *pd) {
          return usesFeatureSendingArgsAndResults(pd);
        }))
      return true;
    if (declUsesFunctionTypesThatUseSending(decl))
      return true;
  }

  // Check if we have a pattern binding decl for a function that has sending
  // parameters and results.
  if (auto *pbd = dyn_cast<PatternBindingDecl>(decl)) {
    for (auto index : range(pbd->getNumPatternEntries())) {
      auto *pattern = pbd->getPattern(index);
      if (pattern->hasType() && isFunctionTypeWithSending(pattern->getType()))
        return true;
    }
  }

  return false;
}

static bool usesFeatureLifetimeDependence(Decl *decl) {
  if (decl->getAttrs().hasAttribute<LifetimeAttr>()) {
    return true;
  }
  if (auto *afd = dyn_cast<AbstractFunctionDecl>(decl)) {
    return afd->getInterfaceType()
      ->getAs<AnyFunctionType>()
      ->hasLifetimeDependencies();
  }
  if (auto *varDecl = dyn_cast<VarDecl>(decl)) {
    return !varDecl->getTypeInContext()->isEscapable();
  }
  return false;
}

UNINTERESTING_FEATURE(DynamicActorIsolation)
UNINTERESTING_FEATURE(NonfrozenEnumExhaustivity)
UNINTERESTING_FEATURE(ClosureIsolation)
UNINTERESTING_FEATURE(Extern)
UNINTERESTING_FEATURE(ConsumeSelfInDeinit)
UNINTERESTING_FEATURE(StrictSendableMetatypes)

static bool usesFeatureBitwiseCopyable2(Decl *decl) {
  if (!decl->getModuleContext()->isStdlibModule()) {
    return false;
  }
  if (auto *proto = dyn_cast<ProtocolDecl>(decl)) {
    return proto->getNameStr() == "BitwiseCopyable";
  }
  if (auto *typealias = dyn_cast<TypeAliasDecl>(decl)) {
    return typealias->getNameStr() == "_BitwiseCopyable";
  }
  return false;
}

static bool usesFeatureIsolatedAny(Decl *decl) {
  return usesTypeMatching(decl, [](Type type) {
    if (auto fnType = type->getAs<AnyFunctionType>()) {
      return fnType->getIsolation().isErased();
    }
    return false;
  });
}

static bool usesFeatureAddressableParameters(Decl *d) {
  if (d->getAttrs().hasAttribute<AddressableSelfAttr>()) {
    return true;
  }

  auto fd = dyn_cast<AbstractFunctionDecl>(d);
  if (!fd) {
    return false;
  }
  
  for (auto pd : *fd->getParameters()) {
    if (pd->isAddressable()) {
      return true;
    }
  }
  return false;
}

static bool usesFeatureAddressableTypes(Decl *d) {
  if (d->getAttrs().hasAttribute<AddressableForDependenciesAttr>()) {
    return true;
  }
  
  return false;
}

UNINTERESTING_FEATURE(IsolatedAny2)
UNINTERESTING_FEATURE(GlobalActorIsolatedTypesUsability)
UNINTERESTING_FEATURE(ObjCImplementation)
UNINTERESTING_FEATURE(ObjCImplementationWithResilientStorage)
UNINTERESTING_FEATURE(CImplementation)
UNINTERESTING_FEATURE(Sensitive)
UNINTERESTING_FEATURE(DebugDescriptionMacro)
UNINTERESTING_FEATURE(ReinitializeConsumeInMultiBlockDefer)
UNINTERESTING_FEATURE(SE427NoInferenceOnExtension)
UNINTERESTING_FEATURE(TrailingComma)
UNINTERESTING_FEATURE(RawIdentifiers)
UNINTERESTING_FEATURE(InferIsolatedConformances)

static ABIAttr *getABIAttr(Decl *decl) {
  if (auto pbd = dyn_cast<PatternBindingDecl>(decl))
    for (auto i : range(pbd->getNumPatternEntries()))
      if (auto anchorVar = pbd->getAnchoringVarDecl(i))
        return getABIAttr(anchorVar);
  // FIXME: EnumCaseDecl/EnumElementDecl

  return decl->getAttrs().getAttribute<ABIAttr>();
}

static bool usesFeatureABIAttribute(Decl *decl) {
  return getABIAttr(decl) != nullptr;
}

static bool usesFeatureIsolatedConformances(Decl *decl) { 
  // FIXME: Check conformances associated with this decl?
  return false;
}

static bool usesFeatureConcurrencySyntaxSugar(Decl *decl) {
  return false;
}

static bool usesFeatureCompileTimeValues(Decl *decl) {
  return decl->getAttrs().hasAttribute<ConstValAttr>() ||
         decl->getAttrs().hasAttribute<ConstInitializedAttr>();
}

static bool usesFeatureClosureBodyMacro(Decl *decl) {
  return false;
}

static bool usesFeatureMemorySafetyAttributes(Decl *decl) {
  if (decl->getAttrs().hasAttribute<SafeAttr>() ||
      decl->getAttrs().hasAttribute<UnsafeAttr>())
    return true;

  IterableDeclContext *idc;
  if (auto nominal = dyn_cast<NominalTypeDecl>(decl))
    idc = nominal;
  else if (auto ext = dyn_cast<ExtensionDecl>(decl))
    idc = ext;
  else
    idc = nullptr;

  // Look for an @unsafe conformance ascribed to this declaration.
  if (idc) {
    auto conformances = idc->getLocalConformances();
    for (auto conformance : conformances) {
      auto rootConformance = conformance->getRootConformance();
      if (auto rootNormalConformance =
              dyn_cast<NormalProtocolConformance>(rootConformance)) {
        if (rootNormalConformance->getExplicitSafety() == ExplicitSafety::Unsafe)
          return true;
      }
    }
  }

  return false;
}

UNINTERESTING_FEATURE(StrictMemorySafety)
UNINTERESTING_FEATURE(SafeInteropWrappers)
UNINTERESTING_FEATURE(AssumeResilientCxxTypes)
UNINTERESTING_FEATURE(ImportNonPublicCxxMembers)
UNINTERESTING_FEATURE(CoroutineAccessorsUnwindOnCallerError)

static bool usesFeatureSwiftSettings(const Decl *decl) {
  // We just need to guard `#SwiftSettings`.
  auto *macro = dyn_cast<MacroDecl>(decl);
  return macro && macro->isStdlibDecl() &&
         macro->getMacroRoles().contains(MacroRole::Declaration) &&
         macro->getBaseIdentifier().is("SwiftSettings");
}

bool swift::usesFeatureIsolatedDeinit(const Decl *decl) {
  if (auto cd = dyn_cast<ClassDecl>(decl)) {
    return cd->getFormalAccess() == AccessLevel::Open &&
           usesFeatureIsolatedDeinit(cd->getDestructor());
  } else if (auto dd = dyn_cast<DestructorDecl>(decl)) {
    if (dd->hasExplicitIsolationAttribute()) {
      return true;
    }
    if (auto superDD = dd->getSuperDeinit()) {
      return usesFeatureIsolatedDeinit(superDD);
    }
    return false;
  } else {
    return false;
  }
}

static bool usesFeatureValueGenerics(Decl *decl) {
  auto genericContext = decl->getAsGenericContext();

  if (!genericContext || !genericContext->getGenericParams())
    return false;

  for (auto param : genericContext->getGenericParams()->getParams()) {
    if (param->isValue())
      return true;

    continue;
  }

  return false;
}

static bool usesFeatureCoroutineAccessors(Decl *decl) {
  auto accessorDeclUsesFeatureCoroutineAccessors = [](AccessorDecl *accessor) {
    return requiresFeatureCoroutineAccessors(accessor->getAccessorKind());
  };
  switch (decl->getKind()) {
  case DeclKind::Var: {
    auto *var = cast<VarDecl>(decl);
    return llvm::any_of(var->getAllAccessors(),
                        accessorDeclUsesFeatureCoroutineAccessors);
  }
  case DeclKind::Accessor: {
    auto *accessor = cast<AccessorDecl>(decl);
    return accessorDeclUsesFeatureCoroutineAccessors(accessor);
  }
  default:
    return false;
  }
}

static bool usesFeatureCustomAvailability(Decl *decl) {
  // FIXME: [availability] Check whether @available attributes for custom
  // domains are attached to the decl.
  return false;
}

static bool usesFeatureBuiltinEmplaceTypedThrows(Decl *decl) {
  // Callers of 'Builtin.emplace' should explicitly guard the usage with #if.
  return false;
}

static bool usesFeatureExecutionAttribute(Decl *decl) {
  if (auto *ASD = dyn_cast<AbstractStorageDecl>(decl)) {
    if (auto *getter = ASD->getAccessor(AccessorKind::Get))
      return usesFeatureExecutionAttribute(getter);
    return false;
  }

  if (decl->getAttrs().hasAttribute<ExecutionAttr>())
    return true;

  auto VD = dyn_cast<ValueDecl>(decl);
  if (!VD)
    return false;

  auto hasExecutionAttr = [](TypeRepr *R) {
    if (!R)
      return false;

    return R->findIf([](TypeRepr *repr) {
      if (auto *AT = dyn_cast<AttributedTypeRepr>(repr)) {
        return llvm::any_of(AT->getAttrs(), [](TypeOrCustomAttr attr) {
          if (auto *TA = attr.dyn_cast<TypeAttribute *>()) {
            return isa<ExecutionTypeAttr>(TA);
          }
          return false;
        });
      }
      return false;
    });
  };

  // Check if any parameters that have `@execution` attribute.
  if (auto *PL = getParameterList(VD)) {
    for (auto *P : *PL) {
      if (hasExecutionAttr(P->getTypeRepr()))
        return true;
    }
  }

  if (hasExecutionAttr(VD->getResultTypeRepr()))
    return true;

  return false;
}

// ----------------------------------------------------------------------------
// MARK: - FeatureSet
// ----------------------------------------------------------------------------

void FeatureSet::collectRequiredFeature(Feature feature,
                                        InsertOrRemove operation) {
  required.insertOrRemove(feature, operation == Insert);
}

void FeatureSet::collectSuppressibleFeature(Feature feature,
                                            InsertOrRemove operation) {
  suppressible.insertOrRemove(numFeatures() - size_t(feature),
                              operation == Insert);
}

static bool hasFeatureSuppressionAttribute(Decl *decl, StringRef featureName,
                                           bool inverted) {
  auto attr = decl->getAttrs().getAttribute<AllowFeatureSuppressionAttr>();
  if (!attr)
    return false;

  if (attr->getInverted() != inverted)
    return false;

  for (auto suppressedFeature : attr->getSuppressedFeatures()) {
    if (suppressedFeature.is(featureName))
      return true;
  }

  return false;
}

static bool disallowFeatureSuppression(StringRef featureName, Decl *decl) {
  return hasFeatureSuppressionAttribute(decl, featureName, true);
}

static bool allowFeatureSuppression(StringRef featureName, Decl *decl) {
  return hasFeatureSuppressionAttribute(decl, featureName, false);
}

/// Go through all the features used by the given declaration and
/// either add or remove them to this set.
void FeatureSet::collectFeaturesUsed(Decl *decl, InsertOrRemove operation) {
  // Count feature usage in an ABI decl as feature usage by the API, not itself,
  // since we can't use `#if` inside an @abi attribute.
  Decl *abiDecl = nullptr;
  if (auto abiAttr = getABIAttr(decl)) {
    abiDecl = abiAttr->abiDecl;
  }

#define CHECK(Function) (Function(decl) || (abiDecl && Function(abiDecl)))
#define CHECK_ARG(Function, Arg) (Function(Arg, decl) || (abiDecl && Function(Arg, abiDecl)))

  // Go through each of the features, checking whether the
  // declaration uses that feature.
#define LANGUAGE_FEATURE(FeatureName, SENumber, Description)                   \
  if (CHECK(usesFeature##FeatureName))                                         \
    collectRequiredFeature(Feature::FeatureName, operation);
#define SUPPRESSIBLE_LANGUAGE_FEATURE(FeatureName, SENumber, Description)      \
  if (CHECK(usesFeature##FeatureName)) {                                       \
    if (CHECK_ARG(disallowFeatureSuppression, #FeatureName))                   \
      collectRequiredFeature(Feature::FeatureName, operation);                 \
    else                                                                       \
      collectSuppressibleFeature(Feature::FeatureName, operation);             \
  }
#define CONDITIONALLY_SUPPRESSIBLE_LANGUAGE_FEATURE(FeatureName, SENumber, Description)      \
  if (CHECK(usesFeature##FeatureName)) {                                       \
    if (CHECK_ARG(allowFeatureSuppression, #FeatureName))                      \
      collectSuppressibleFeature(Feature::FeatureName, operation);             \
    else                                                                       \
      collectRequiredFeature(Feature::FeatureName, operation);                 \
  }
#include "swift/Basic/Features.def"
#undef CHECK
#undef CHECK_ARG
}

FeatureSet swift::getUniqueFeaturesUsed(Decl *decl) {
  // Add all the features used by this declaration.
  FeatureSet features;
  features.collectFeaturesUsed(decl, FeatureSet::Insert);

  // Remove all the features used by all enclosing declarations.
  Decl *enclosingDecl = decl;
  while (!features.empty()) {
    // Find the next outermost enclosing declaration.
    if (auto accessor = dyn_cast<AccessorDecl>(enclosingDecl))
      enclosingDecl = accessor->getStorage();
    else
      enclosingDecl = enclosingDecl->getDeclContext()->getAsDecl();
    if (!enclosingDecl)
      break;

    features.collectFeaturesUsed(enclosingDecl, FeatureSet::Remove);
  }

  return features;
}
