// RUN: %swift -target x86_64-apple-macosx10.9 -emit-ir -parse-stdlib -primary-file %s | %FileCheck --check-prefix=CHECK --check-prefix=CHECK-64 %s
// RUN: %swift -target i386-apple-ios7.0 -emit-ir -parse-stdlib -primary-file %s | %FileCheck --check-prefix=CHECK --check-prefix=CHECK-32 %s
// RUN: %swift -target x86_64-apple-ios7.0 -emit-ir -parse-stdlib -primary-file %s | %FileCheck --check-prefix=CHECK --check-prefix=CHECK-64 %s
// RUN: %swift -target i386-apple-tvos9.0 -emit-ir -parse-stdlib -primary-file %s | %FileCheck --check-prefix=CHECK --check-prefix=CHECK-32 %s
// RUN: %swift -target x86_64-apple-tvos9.0 -emit-ir -parse-stdlib -primary-file %s | %FileCheck --check-prefix=CHECK --check-prefix=CHECK-64 %s
// RUN: %swift -target i386-apple-watchos2.0 -emit-ir -parse-stdlib -primary-file %s | %FileCheck --check-prefix=CHECK --check-prefix=CHECK-32 %s
// RUN: %swift -target x86_64-unknown-linux-gnu -disable-objc-interop -emit-ir -parse-stdlib -primary-file %s | %FileCheck --check-prefix=CHECK --check-prefix=CHECK-64 %s

// REQUIRES: CODEGENERATOR=X86

// CHECK: define hidden swiftcc %swift.type* [[GENERIC_TYPEOF:@"\$S17generic_metatypes0A6TypeofyxmxlF"]](%swift.opaque* noalias nocapture, %swift.type* [[TYPE:%.*]])
func genericTypeof<T>(_ x: T) -> T.Type {
  // CHECK: [[METATYPE:%.*]] = call %swift.type* @swift_getDynamicType(%swift.opaque* {{.*}}, %swift.type* [[TYPE]], i1 false)
  // CHECK: ret %swift.type* [[METATYPE]]
  return type(of: x)
}

struct Foo {}
class Bar {}

// CHECK-LABEL: define hidden swiftcc %swift.type* @"$S17generic_metatypes27remapToSubstitutedMetatypes{{.*}}"(%T17generic_metatypes3BarC*) {{.*}} {
func remapToSubstitutedMetatypes(_ x: Foo, y: Bar)
  -> (Foo.Type, Bar.Type)
{
  // CHECK: call swiftcc %swift.type* [[GENERIC_TYPEOF]](%swift.opaque* noalias nocapture undef, %swift.type* {{.*}} @"$S17generic_metatypes3FooVMf", {{.*}})
  // CHECK: [[T0:%.*]] = call %swift.type* @"$S17generic_metatypes3BarCMa"()
  // CHECK: [[BAR_META:%.*]] = call swiftcc %swift.type* [[GENERIC_TYPEOF]](%swift.opaque* noalias nocapture {{%.*}}, %swift.type* [[T0]])
  // CHECK: ret %swift.type* [[BAR_META]]
  return (genericTypeof(x), genericTypeof(y))
}


// CHECK-LABEL: define hidden swiftcc void @"$S17generic_metatypes23remapToGenericMetatypesyyF"()
func remapToGenericMetatypes() {
  // CHECK: [[T0:%.*]] = call %swift.type* @"$S17generic_metatypes3BarCMa"()
  // CHECK: call swiftcc void @"$S17generic_metatypes0A9Metatypes{{.*}}"(%swift.type* {{.*}} @"$S17generic_metatypes3FooVMf", {{.*}} %swift.type* [[T0]], %swift.type* {{.*}} @"$S17generic_metatypes3FooVMf", {{.*}} %swift.type* [[T0]])
  genericMetatypes(Foo.self, Bar.self)
}

func genericMetatypes<T, U>(_ t: T.Type, _ u: U.Type) {}

protocol Bas {}

// CHECK: define hidden swiftcc { %swift.type*, i8** } @"$S17generic_metatypes14protocolTypeof{{.*}}"(%T17generic_metatypes3BasP* noalias nocapture dereferenceable({{.*}}))
func protocolTypeof(_ x: Bas) -> Bas.Type {
  // CHECK: [[METADATA_ADDR:%.*]] = getelementptr inbounds %T17generic_metatypes3BasP, %T17generic_metatypes3BasP* [[X:%.*]], i32 0, i32 1
  // CHECK: [[METADATA:%.*]] = load %swift.type*, %swift.type** [[METADATA_ADDR]]
  // CHECK: [[BUFFER:%.*]] = getelementptr inbounds %T17generic_metatypes3BasP, %T17generic_metatypes3BasP* [[X]], i32 0, i32 0
  // CHECK: [[VALUE_ADDR:%.*]] = call %swift.opaque* @__swift_project_boxed_opaque_existential_1({{.*}} [[BUFFER]], %swift.type* [[METADATA]])
  // CHECK: [[METATYPE:%.*]] = call %swift.type* @swift_getDynamicType(%swift.opaque* [[VALUE_ADDR]], %swift.type* [[METADATA]], i1 true)
  // CHECK: [[WTABLE_ADDR:%.*]] = getelementptr inbounds %T17generic_metatypes3BasP, %T17generic_metatypes3BasP* %0, i32 0, i32 2
  // CHECK: [[WTABLE:%.*]] = load i8**, i8*** [[WTABLE_ADDR]]
  // CHECK-NOT: call void @__swift_destroy_boxed_opaque_existential_1(%T17generic_metatypes3BasP* %0)
  // CHECK: [[T0:%.*]] = insertvalue { %swift.type*, i8** } undef, %swift.type* [[METATYPE]], 0
  // CHECK: [[T1:%.*]] = insertvalue { %swift.type*, i8** } [[T0]], i8** [[WTABLE]], 1
  // CHECK: ret { %swift.type*, i8** } [[T1]]
  return type(of: x)
}

struct Zim : Bas {}
class Zang : Bas {}

// CHECK-LABEL: define hidden swiftcc { %swift.type*, i8** } @"$S17generic_metatypes15metatypeErasureyAA3Bas_pXpAA3ZimVmF"() #0
func metatypeErasure(_ z: Zim.Type) -> Bas.Type {
  // CHECK: ret { %swift.type*, i8** } {{.*}} @"$S17generic_metatypes3ZimVMf", {{.*}} @"$S17generic_metatypes3ZimVAA3BasAAWP"
  return z
}

// CHECK-LABEL: define hidden swiftcc { %swift.type*, i8** } @"$S17generic_metatypes15metatypeErasureyAA3Bas_pXpAA4ZangCmF"(%swift.type*) #0
func metatypeErasure(_ z: Zang.Type) -> Bas.Type {
  // CHECK: [[RET:%.*]] = insertvalue { %swift.type*, i8** } undef, %swift.type* %0, 0
  // CHECK: [[RET2:%.*]] = insertvalue { %swift.type*, i8** } [[RET]], i8** getelementptr inbounds ([1 x i8*], [1 x i8*]* @"$S17generic_metatypes4ZangCAA3BasAAWP", i32 0, i32 0), 1
  // CHECK: ret { %swift.type*, i8** } [[RET2]]
  return z
}

struct OneArg<T> {}
struct TwoArgs<T, U> {}
struct ThreeArgs<T, U, V> {}
struct FourArgs<T, U, V, W> {}
struct FiveArgs<T, U, V, W, X> {}

func genericMetatype<A>(_ x: A.Type) {}

// CHECK-LABEL: define hidden swiftcc void @"$S17generic_metatypes20makeGenericMetatypesyyF"() {{.*}} {
func makeGenericMetatypes() {
  // CHECK: call %swift.type* @"$S17generic_metatypes6OneArgVyAA3FooVGMa"() [[NOUNWIND_READNONE:#[0-9]+]]
  genericMetatype(OneArg<Foo>.self)

  // CHECK: call %swift.type* @"$S17generic_metatypes7TwoArgsVyAA3FooVAA3BarCGMa"() [[NOUNWIND_READNONE]]
  genericMetatype(TwoArgs<Foo, Bar>.self)

  // CHECK: call %swift.type* @"$S17generic_metatypes9ThreeArgsVyAA3FooVAA3BarCAEGMa"() [[NOUNWIND_READNONE]]
  genericMetatype(ThreeArgs<Foo, Bar, Foo>.self)

  // CHECK: call %swift.type* @"$S17generic_metatypes8FourArgsVyAA3FooVAA3BarCAeGGMa"() [[NOUNWIND_READNONE]]
  genericMetatype(FourArgs<Foo, Bar, Foo, Bar>.self)

  // CHECK: call %swift.type* @"$S17generic_metatypes8FiveArgsVyAA3FooVAA3BarCAegEGMa"() [[NOUNWIND_READNONE]]
  genericMetatype(FiveArgs<Foo, Bar, Foo, Bar, Foo>.self)
}

// CHECK: define linkonce_odr hidden %swift.type* @"$S17generic_metatypes6OneArgVyAA3FooVGMa"() [[NOUNWIND_READNONE_OPT:#[0-9]+]]
// CHECK:   call %swift.type* @"$S17generic_metatypes6OneArgVMa"(%swift.type* {{.*}} @"$S17generic_metatypes3FooVMf", {{.*}}) [[NOUNWIND_READNONE:#[0-9]+]]

// CHECK-LABEL: define hidden %swift.type* @"$S17generic_metatypes6OneArgVMa"(%swift.type*)
// CHECK:   [[BUFFER:%.*]] = alloca { %swift.type* }
// CHECK:   [[BUFFER_PTR:%.*]] = bitcast { %swift.type* }* [[BUFFER]] to i8*
// CHECK:   call void @llvm.lifetime.start
// CHECK:   [[BUFFER_ELT:%.*]] = getelementptr inbounds { %swift.type* }, { %swift.type* }* [[BUFFER]], i32 0, i32 0
// CHECK:   store %swift.type* %0, %swift.type** [[BUFFER_ELT]]
// CHECK:   [[BUFFER_PTR:%.*]] = bitcast { %swift.type* }* [[BUFFER]] to i8*
// CHECK:   [[METADATA:%.*]] = call %swift.type* @swift_getGenericMetadata(%swift.type_descriptor* {{.*}} @"$S17generic_metatypes6OneArgVMn" {{.*}}, i8* [[BUFFER_PTR]])
// CHECK:   [[BUFFER_PTR:%.*]] = bitcast { %swift.type* }* [[BUFFER]] to i8*
// CHECK:   call void @llvm.lifetime.end
// CHECK:   ret %swift.type* [[METADATA]]

// CHECK: define linkonce_odr hidden %swift.type* @"$S17generic_metatypes7TwoArgsVyAA3FooVAA3BarCGMa"() [[NOUNWIND_READNONE_OPT]]
// CHECK:   [[T0:%.*]] = call %swift.type* @"$S17generic_metatypes3BarCMa"()
// CHECK:   call %swift.type* @"$S17generic_metatypes7TwoArgsVMa"(%swift.type* {{.*}} @"$S17generic_metatypes3FooVMf", {{.*}}, %swift.type* [[T0]])

// CHECK-LABEL: define hidden %swift.type* @"$S17generic_metatypes7TwoArgsVMa"(%swift.type*, %swift.type*)
// CHECK:   [[BUFFER:%.*]] = alloca { %swift.type*, %swift.type* }
// CHECK:   [[BUFFER_PTR:%.*]] = bitcast { %swift.type*, %swift.type* }* [[BUFFER]] to i8*
// CHECK:   call void @llvm.lifetime.start
// CHECK:   [[BUFFER_ELT:%.*]] = getelementptr inbounds { %swift.type*, %swift.type* }, { %swift.type*, %swift.type* }* [[BUFFER]], i32 0, i32 0
// CHECK:   store %swift.type* %0, %swift.type** [[BUFFER_ELT]]
// CHECK:   [[BUFFER_ELT:%.*]] = getelementptr inbounds { %swift.type*, %swift.type* }, { %swift.type*, %swift.type* }* [[BUFFER]], i32 0, i32 1
// CHECK:   store %swift.type* %1, %swift.type** [[BUFFER_ELT]]
// CHECK:   [[BUFFER_PTR:%.*]] = bitcast { %swift.type*, %swift.type* }* [[BUFFER]] to i8*
// CHECK:   [[METADATA:%.*]] = call %swift.type* @swift_getGenericMetadata(%swift.type_descriptor* {{.*}} @"$S17generic_metatypes7TwoArgsVMn" {{.*}}, i8* [[BUFFER_PTR]])
// CHECK:   [[BUFFER_PTR:%.*]] = bitcast { %swift.type*, %swift.type* }* [[BUFFER]] to i8*
// CHECK:   call void @llvm.lifetime.end
// CHECK:   ret %swift.type* [[METADATA]]

// CHECK: define linkonce_odr hidden %swift.type* @"$S17generic_metatypes9ThreeArgsVyAA3FooVAA3BarCAEGMa"() [[NOUNWIND_READNONE_OPT]]
// CHECK:   [[T0:%.*]] = call %swift.type* @"$S17generic_metatypes3BarCMa"()
// CHECK:   call %swift.type* @"$S17generic_metatypes9ThreeArgsVMa"(%swift.type* {{.*}} @"$S17generic_metatypes3FooVMf", {{.*}}, %swift.type* [[T0]], %swift.type* {{.*}} @"$S17generic_metatypes3FooVMf", {{.*}}) [[NOUNWIND_READNONE]]

// CHECK-LABEL: define hidden %swift.type* @"$S17generic_metatypes9ThreeArgsVMa"(%swift.type*, %swift.type*, %swift.type*)
// CHECK:   [[BUFFER:%.*]] = alloca { %swift.type*, %swift.type*, %swift.type* }
// CHECK:   [[BUFFER_PTR:%.*]] = bitcast { %swift.type*, %swift.type*, %swift.type* }* [[BUFFER]] to i8*
// CHECK:   call void @llvm.lifetime.start
// CHECK:   [[BUFFER_ELT:%.*]] = getelementptr inbounds { %swift.type*, %swift.type*, %swift.type* }, { %swift.type*, %swift.type*, %swift.type* }* [[BUFFER]], i32 0, i32 0
// CHECK:   store %swift.type* %0, %swift.type** [[BUFFER_ELT]]
// CHECK:   [[BUFFER_ELT:%.*]] = getelementptr inbounds { %swift.type*, %swift.type*, %swift.type* }, { %swift.type*, %swift.type*, %swift.type* }* [[BUFFER]], i32 0, i32 1
// CHECK:   store %swift.type* %1, %swift.type** [[BUFFER_ELT]]
// CHECK:   [[BUFFER_ELT:%.*]] = getelementptr inbounds { %swift.type*, %swift.type*, %swift.type* }, { %swift.type*, %swift.type*, %swift.type* }* [[BUFFER]], i32 0, i32 2
// CHECK:   store %swift.type* %2, %swift.type** [[BUFFER_ELT]]
// CHECK:   [[BUFFER_PTR:%.*]] = bitcast { %swift.type*, %swift.type*, %swift.type* }* [[BUFFER]] to i8*
// CHECK:   [[METADATA:%.*]] = call %swift.type* @swift_getGenericMetadata(%swift.type_descriptor* {{.*}} @"$S17generic_metatypes9ThreeArgsVMn" {{.*}}, i8* [[BUFFER_PTR]])
// CHECK:   [[BUFFER_PTR:%.*]] = bitcast { %swift.type*, %swift.type*, %swift.type* }* [[BUFFER]] to i8*
// CHECK:   call void @llvm.lifetime.end
// CHECK:   ret %swift.type* [[METADATA]]

// CHECK: define linkonce_odr hidden %swift.type* @"$S17generic_metatypes8FourArgsVyAA3FooVAA3BarCAeGGMa"() [[NOUNWIND_READNONE_OPT]]
// CHECK:   [[BUFFER:%.*]] = alloca [4 x i8*]
// CHECK:   [[T0:%.*]] = call %swift.type* @"$S17generic_metatypes3BarCMa"()
// CHECK:   call void @llvm.lifetime.start
// CHECK-NEXT: [[SLOT_3:%.*]] = getelementptr inbounds [4 x i8*], [4 x i8*]* [[BUFFER]], i32 0, i32 3
// CHECK-NEXT:  [[T0_AS_VOIDPTR:%.*]] = bitcast %swift.type* [[T0]] to i8*
// CHECK-NEXT:  store i8* [[T0_AS_VOIDPTR]], i8** [[SLOT_3]]
// CHECK-NEXT:  [[BUFFER_PTR:%.*]] = bitcast [4 x i8*]* [[BUFFER]] to i8**
// CHECK-NEXT:   call %swift.type* @"$S17generic_metatypes8FourArgsVMa"(%swift.type* {{.*}} @"$S17generic_metatypes3FooVMf", {{.*}}, %swift.type* [[T0]], %swift.type* {{.*}} @"$S17generic_metatypes3FooVMf", {{.*}}, i8** [[BUFFER_PTR]]) [[NOUNWIND_ARGMEM:#[0-9]+]]
// CHECK: call void @llvm.lifetime.end.p0i8

// CHECK: define linkonce_odr hidden %swift.type* @"$S17generic_metatypes8FiveArgsVyAA3FooVAA3BarCAegEGMa"() [[NOUNWIND_READNONE_OPT]]
// CHECK:   [[T0:%.*]] = call %swift.type* @"$S17generic_metatypes3BarCMa"()
// CHECK:   call %swift.type* @"$S17generic_metatypes8FiveArgsVMa"(%swift.type* {{.*}} @"$S17generic_metatypes3FooVMf", {{.*}}, %swift.type* [[T0]], %swift.type* {{.*}} @"$S17generic_metatypes3FooVMf", {{.*}}, i8**

// CHECK: define hidden %swift.type* @"$S17generic_metatypes8FiveArgsVMa"(%swift.type*, %swift.type*, %swift.type*, i8**) [[NOUNWIND_OPT:#[0-9]+]]
// CHECK-NOT: alloc
// CHECK:   call %swift.type* @swift_getGenericMetadata(%swift.type_descriptor* {{.*}} @"$S17generic_metatypes8FiveArgsVMn" {{.*}}, i8*
// CHECK-NOT: call void @llvm.lifetime.end
// CHECK:   ret %swift.type*

// CHECK: attributes [[NOUNWIND_READNONE_OPT]] = { nounwind readnone "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "target-cpu"
// CHECK: attributes [[NOUNWIND_OPT]] = { nounwind "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "target-cpu"
// CHECK: attributes [[NOUNWIND_READNONE]] = { nounwind readnone }
// CHECK: attributes [[NOUNWIND_ARGMEM]] = { inaccessiblemem_or_argmemonly nounwind }
