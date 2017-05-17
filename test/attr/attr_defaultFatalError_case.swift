enum Runcible {
  case spoon
  case hat
  // @_defaultFatalError(swift, introduced: 4)
  case fork
}

enum Trioptional {
  case some(Runcible)
  case just(Runcible)
  case none
}

enum Fungible {
  case cash
  case giftCard
}

func missingCases(x: Runcible?, y: Runcible?, z: Trioptional) {
  // Should warn in S3 mode that `fork` isn't used
  switch x! {
  case .spoon:
    break
  case .hat:
    break
  }

  // Should warn in S3 mode that `fork` isn't used
  switch (x!, y!) {
  case (.spoon, .spoon):
    break
  case (.spoon, .hat):
    break
  case (.hat, .spoon):
    break
  case (.hat, .hat):
    break
  }

  // Should error, since `fork` is used but not totally covered
  switch (x!, y!) {
  case (.spoon, .spoon):
    break
  case (.spoon, .hat):
    break
  case (.hat, .spoon):
    break
  case (.hat, .hat):
    break
  case (.fork, .spoon):
    break
  }

  // Should error, since `fork` is used but not totally covered
  switch (x!, y!) {
  case (.spoon, .spoon):
    break
  case (.spoon, .hat):
    break
  case (.hat, .spoon):
    break
  case (.hat, .hat):
    break
  case (.hat, .fork):
    break
  }

  // Should warn in S3 mode that `fork` isn't used
  switch x {
  case .some(.spoon):
    break
  case .some(.hat):
    break
  case .none:
    break
  }
  
  // Should warn in S3 mode that `fork` isn't used
  switch (x, y!) {
  case (.some(.spoon), .spoon):
    break
  case (.some(.spoon), .hat):
    break
  case (.some(.hat), .spoon):
    break
  case (.some(.hat), .hat):
    break
  case (.none, .spoon):
    break
  case (.none, .hat):
    break
  }

  // Should warn in S3 mode that `fork` isn't used
  switch (x, y) {
  case (.some(.spoon), .some(.spoon)):
    break
  case (.some(.spoon), .some(.hat)):
    break
  case (.some(.spoon), .none):
    break
  case (.some(.hat), .some(.spoon)):
    break
  case (.some(.hat), .some(.hat)):
    break
  case (.some(.hat), .none):
    break
  case (.some(.hat), .some(.spoon)):
    break
  case (.some(.hat), .some(.hat)):
    break
  case (.some(.hat), .none):
    break
  }

  // Should warn in S3 mode that `fork` isn't used
  switch z {
  case .some(.spoon):
    break
  case .some(.hat):
    break
  case .just(.spoon):
    break
  case .just(.hat):
    break
  case .none:
    break
  }
}
