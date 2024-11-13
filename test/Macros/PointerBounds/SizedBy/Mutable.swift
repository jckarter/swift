// REQUIRES: swift_swift_parser
// REQUIRES: pointer_bounds

// RUN: %target-swift-frontend %s -swift-version 5 -module-name main -disable-availability-checking -typecheck -plugin-path %swift-plugin-dir -dump-macro-expansions 2>&1 | %FileCheck --match-full-lines %s

@PointerBounds(.sizedBy(pointer: 1, size: "size"))
func myFunc(_ ptr: UnsafeMutableRawPointer, _ size: CInt) {
}

// CHECK:      @_alwaysEmitIntoClient
// CHECK-NEXT: func myFunc(_ ptr: UnsafeMutableRawBufferPointer) {
// CHECK-NEXT:     return myFunc(ptr.baseAddress!, CInt(exactly: ptr.count)!)
// CHECK-NEXT: }
