// RUN: %target-swift-frontend -emit-silgen -enable-sil-ownership %s | %FileCheck %s

var escapeHatch: Any = 0

// CHECK-LABEL: sil hidden @$S25without_actually_escaping9letEscape1fyycyyXE_tF
func letEscape(f: () -> ()) -> () -> () {
  // CHECK: bb0([[ARG:%.*]] : @trivial $@noescape @callee_guaranteed () -> ()):
  // TODO: Use a canary wrapper instead of just copying the nonescaping value
  // CHECK: [[ESCAPABLE_COPY:%.*]] = partial_apply [callee_guaranteed] {{%.*}}([[ARG]])
  // CHECK: [[MD_ESCAPABLE_COPY:%.*]] = mark_dependence [[ESCAPABLE_COPY]]
  // CHECK: [[BORROW_MD_ESCAPABLE_COPY:%.*]] = begin_borrow [[MD_ESCAPABLE_COPY]]
  // CHECK: [[SUB_CLOSURE:%.*]] = function_ref @
  // CHECK: [[RESULT:%.*]] = apply [[SUB_CLOSURE]]([[BORROW_MD_ESCAPABLE_COPY]])
  // CHECK: destroy_value [[MD_ESCAPABLE_COPY]]
  // CHECK: return [[RESULT]]
  return withoutActuallyEscaping(f) { return $0 }
}
