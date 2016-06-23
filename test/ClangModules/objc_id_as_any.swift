// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk) -parse -verify -enable-id-as-any %s

import Foundation

func assertTypeIsAny(_: Any.Protocol) {}
func staticType<T>(_: T) -> T.Type { return T.self }

let idLover = IdLover()

let t1 = staticType(idLover.makesId())
assertTypeIsAny(t1)

struct ArbitraryThing {}
idLover.takesId(ArbitraryThing())

var x: AnyObject = NSObject()
idLover.takesArray(ofId: &x)
var y: Any = NSObject()
idLover.takesArray(ofId: &y) // expected-error{{}}
