// RUN: %empty-directory(%t)
// RUN: %target-swift-frontend -emit-module -enable-resilience -emit-module-path=%t/resilient_struct.swiftmodule -module-name=resilient_struct %S/../Inputs/resilient_struct.swift
// RUN: %target-swift-frontend -emit-module -enable-resilience -emit-module-path=%t/resilient_enum.swiftmodule -module-name=resilient_enum -I %t %S/../Inputs/resilient_enum.swift
// RUN: %target-swift-frontend -I %t -emit-ir -enable-resilience %s | %FileCheck %s
// RUN: %target-swift-frontend -I %t -emit-ir -enable-resilience -O %s

import resilient_struct
import resilient_enum

// CHECK: %TSi = type <{ [[INT:i32|i64]] }>

// CHECK-LABEL: @"$S17struct_resilience26StructWithResilientStorageVMf" = internal global

// Resilient structs from outside our resilience domain are manipulated via
// value witnesses

// CHECK-LABEL: define{{( protected)?}} swiftcc void @"$S17struct_resilience26functionWithResilientTypes_1f010resilient_A04SizeVAF_A2FXEtF"(%swift.opaque* noalias nocapture sret, %swift.opaque* noalias nocapture, i8*, %swift.opaque*)

public func functionWithResilientTypes(_ s: Size, f: (Size) -> Size) -> Size {
// CHECK: entry:
// CHECK-NEXT: [[FN:%.*]] = bitcast i8* %2 to void (%swift.opaque*, %swift.opaque*, %swift.refcounted*)*
// CHECK-NEXT: [[SELF:%.*]] = bitcast %swift.opaque* %3 to %swift.refcounted*
// CHECK-NEXT: call swiftcc void [[FN]](%swift.opaque* noalias nocapture sret %0, %swift.opaque* noalias nocapture %1, %swift.refcounted* swiftself [[SELF]])
// CHECK-NEXT: ret void

  return f(s)
}

// Rectangle has fixed layout inside its resilience domain, and dynamic
// layout on the outside.
//
// Make sure we use a type metadata accessor function, and load indirect
// field offsets from it.

// CHECK-LABEL: define{{( protected)?}} swiftcc void @"$S17struct_resilience26functionWithResilientTypesyy010resilient_A09RectangleVF"(%T16resilient_struct9RectangleV* noalias nocapture)
public func functionWithResilientTypes(_ r: Rectangle) {

// CHECK: [[METADATA:%.*]] = call %swift.type* @"$S16resilient_struct9RectangleVMa"()
// CHECK-NEXT: [[METADATA_ADDR:%.*]] = bitcast %swift.type* [[METADATA]] to [[INT]]*
// CHECK-NEXT: [[FIELD_OFFSET_VECTOR:%.*]] = getelementptr inbounds [[INT]], [[INT]]* [[METADATA_ADDR]], [[INT]] 2
// CHECK-NEXT: [[FIELD_OFFSET_PTR:%.*]] = getelementptr inbounds [[INT]], [[INT]]* [[FIELD_OFFSET_VECTOR]], i32 2
// CHECK-NEXT: [[FIELD_OFFSET:%.*]] = load [[INT]], [[INT]]* [[FIELD_OFFSET_PTR]]
// CHECK-NEXT: [[STRUCT_ADDR:%.*]] = bitcast %T16resilient_struct9RectangleV* %0 to i8*
// CHECK-NEXT: [[FIELD_ADDR:%.*]] = getelementptr inbounds i8, i8* [[STRUCT_ADDR]], [[INT]] [[FIELD_OFFSET]]
// CHECK-NEXT: [[FIELD_PTR:%.*]] = bitcast i8* [[FIELD_ADDR]] to %TSi*
// CHECK-NEXT: [[FIELD_PAYLOAD_PTR:%.*]] = getelementptr inbounds %TSi, %TSi* [[FIELD_PTR]], i32 0, i32 0
// CHECK-NEXT: [[FIELD_PAYLOAD:%.*]] = load [[INT]], [[INT]]* [[FIELD_PAYLOAD_PTR]]

  _ = r.color

// CHECK-NEXT: ret void

}

// Resilient structs from inside our resilience domain are manipulated
// directly.

public struct MySize {
  public let w: Int
  public let h: Int
}

// CHECK-LABEL: define{{( protected)?}} swiftcc void @"$S17struct_resilience28functionWithMyResilientTypes_1fAA0E4SizeVAE_A2EXEtF"(%T17struct_resilience6MySizeV* {{.*}}, %T17struct_resilience6MySizeV* {{.*}}, i8*, %swift.opaque*)
public func functionWithMyResilientTypes(_ s: MySize, f: (MySize) -> MySize) -> MySize {
// CHECK: entry:
// CHECK-NEXT: [[FN:%.*]] = bitcast i8* %2
// CHECK-NEXT: [[SELF:%.*]] = bitcast %swift.opaque* %3
// CHECK-NEXT: call swiftcc void [[FN]](%T17struct_resilience6MySizeV* {{.*}} %0, {{.*}} %1, %swift.refcounted* swiftself [[SELF]])
// CHECK-NEXT: ret void
  return f(s)
}

// CHECK-LABEL: declare %swift.type* @"$S16resilient_struct4SizeVMa"()

// Structs with resilient storage from a different resilience domain require
// runtime metadata instantiation, just like generics.

public struct StructWithResilientStorage {
  public let s: Size
  public let ss: (Size, Size)
  public let n: Int
  public let i: ResilientInt
}

// Make sure we call a function to access metadata of structs with
// resilient layout, and go through the field offset vector in the
// metadata when accessing stored properties.

// CHECK-LABEL: define{{( protected)?}} swiftcc {{i32|i64}} @"$S17struct_resilience26StructWithResilientStorageV1nSivg"(%T17struct_resilience26StructWithResilientStorageV* {{.*}})
// CHECK: [[METADATA:%.*]] = call %swift.type* @"$S17struct_resilience26StructWithResilientStorageVMa"()
// CHECK-NEXT: [[METADATA_ADDR:%.*]] = bitcast %swift.type* [[METADATA]] to [[INT]]*
// CHECK-NEXT: [[FIELD_OFFSET_VECTOR:%.*]] = getelementptr inbounds [[INT]], [[INT]]* [[METADATA_ADDR]], [[INT]] 2
// CHECK-NEXT: [[FIELD_OFFSET_PTR:%.*]] = getelementptr inbounds [[INT]], [[INT]]* [[FIELD_OFFSET_VECTOR]], i32 2
// CHECK-NEXT: [[FIELD_OFFSET:%.*]] = load [[INT]], [[INT]]* [[FIELD_OFFSET_PTR]]
// CHECK-NEXT: [[STRUCT_ADDR:%.*]] = bitcast %T17struct_resilience26StructWithResilientStorageV* %0 to i8*
// CHECK-NEXT: [[FIELD_ADDR:%.*]] = getelementptr inbounds i8, i8* [[STRUCT_ADDR]], [[INT]] [[FIELD_OFFSET]]
// CHECK-NEXT: [[FIELD_PTR:%.*]] = bitcast i8* [[FIELD_ADDR]] to %TSi*
// CHECK-NEXT: [[FIELD_PAYLOAD_PTR:%.*]] = getelementptr inbounds %TSi, %TSi* [[FIELD_PTR]], i32 0, i32 0
// CHECK-NEXT: [[FIELD_PAYLOAD:%.*]] = load [[INT]], [[INT]]* [[FIELD_PAYLOAD_PTR]]
// CHECK-NEXT: ret [[INT]] [[FIELD_PAYLOAD]]


// Indirect enums with resilient payloads are still fixed-size.

public struct StructWithIndirectResilientEnum {
  public let s: FunnyShape
  public let n: Int
}


// CHECK-LABEL: define{{( protected)?}} swiftcc {{i32|i64}} @"$S17struct_resilience31StructWithIndirectResilientEnumV1nSivg"(%T17struct_resilience31StructWithIndirectResilientEnumV* {{.*}})
// CHECK: [[FIELD_PTR:%.*]] = getelementptr inbounds %T17struct_resilience31StructWithIndirectResilientEnumV, %T17struct_resilience31StructWithIndirectResilientEnumV* %0, i32 0, i32 1
// CHECK-NEXT: [[FIELD_PAYLOAD_PTR:%.*]] = getelementptr inbounds %TSi, %TSi* [[FIELD_PTR]], i32 0, i32 0
// CHECK-NEXT: [[FIELD_PAYLOAD:%.*]] = load [[INT]], [[INT]]* [[FIELD_PAYLOAD_PTR]]
// CHECK-NEXT: ret [[INT]] [[FIELD_PAYLOAD]]


// Partial application of methods on resilient value types

public struct ResilientStructWithMethod {
  public func method() {}
}

// Corner case -- type is address-only in SIL, but empty in IRGen

// CHECK-LABEL: define{{( protected)?}} swiftcc void @"$S17struct_resilience29partialApplyOfResilientMethod1ryAA0f10StructWithG0V_tF"(%T17struct_resilience25ResilientStructWithMethodV* noalias nocapture)
public func partialApplyOfResilientMethod(r: ResilientStructWithMethod) {
  _ = r.method
}

// Type is address-only in SIL, and resilient in IRGen

// CHECK-LABEL: define{{( protected)?}} swiftcc void @"$S17struct_resilience29partialApplyOfResilientMethod1sy010resilient_A04SizeV_tF"(%swift.opaque* noalias nocapture)
public func partialApplyOfResilientMethod(s: Size) {
  _ = s.method
}

// Public metadata accessor for our resilient struct

// CHECK-LABEL: define{{( protected)?}} %swift.type* @"$S17struct_resilience6MySizeVMa"()
// CHECK: ret %swift.type* bitcast ([[INT]]* getelementptr inbounds {{.*}} @"$S17struct_resilience6MySizeVMf", i32 0, i32 1) to %swift.type*)


// CHECK-LABEL: define{{( protected)?}} private void @initialize_metadata_StructWithResilientStorage(i8*)
// CHECK: [[FIELDS:%.*]] = alloca [4 x i8**]

// CHECK: [[FIELDS_ADDR:%.*]] = getelementptr inbounds [4 x i8**], [4 x i8**]* [[FIELDS]], i32 0, i32 0

// public let s: Size

// CHECK: [[FIELD_1:%.*]] = getelementptr inbounds i8**, i8*** [[FIELDS_ADDR]], i32 0
// CHECK: store i8** [[SIZE_AND_ALIGNMENT:%.*]], i8*** [[FIELD_1]]

// public let ss: (Size, Size)

// CHECK: [[FIELD_2:%.*]] = getelementptr inbounds i8**, i8*** [[FIELDS_ADDR]], i32 1
// CHECK: store i8** [[SIZE_AND_ALIGNMENT:%.*]], i8*** [[FIELD_2]]

// Fixed-layout aggregate -- we can reference a static value witness table
// public let n: Int

// CHECK: [[FIELD_3:%.*]] = getelementptr inbounds i8**, i8*** [[FIELDS_ADDR]], i32 2
// CHECK: store i8** getelementptr inbounds (i8*, i8** @"$SBi{{32|64}}_WV", i32 {{.*}}), i8*** [[FIELD_3]]

// Resilient aggregate with one field -- make sure we don't look inside it
// public let i: ResilientInt
// CHECK: [[FIELD_4:%.*]] = getelementptr inbounds i8**, i8*** [[FIELDS_ADDR]], i32 3
// CHECK: store i8** [[SIZE_AND_ALIGNMENT:%.*]], i8*** [[FIELD_4]]

// CHECK: call void @swift_initStructMetadata(%swift.type* {{.*}}, [[INT]] 256, [[INT]] 4, i8*** [[FIELDS_ADDR]], [[INT]]* {{.*}})
// CHECK: store atomic %swift.type* {{.*}} @"$S17struct_resilience26StructWithResilientStorageVMf{{.*}}, %swift.type** @"$S17struct_resilience26StructWithResilientStorageVML" release,
// CHECK: ret void
