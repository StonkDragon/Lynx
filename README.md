# Lynx Config Language
Its just a config file bro...
Well, it can do a bit more than that... like [FizzBuzz](./examples/fizzbuzz.lynx)!

## Syntax
The syntax is pretty simple, it is a key-value pair separated by a colon. The key is the name of the variable and the value is the value of the variable. The value can be a string, number, boolean, or an array, or an expression.

### Example
```
-- This is a comment
foo: {
    bar: 1
    baz: "Hello, World!"
    a-list: [
        "Hello"
        "World"
    ]
}
-- Concatenating multiple strings
a-string: (
    "Hello, "
    "World!"
)
```
Any values contained in `()` get concatenated into a single value. All values must be the same type, with the exception of numbers, which can convert into strings.

### Types
- String: `"Hello, World!"`
- Number: `1`/`2.34`/`true`/`false`
  - Booleans are considered numbers
- List: `[1, 2, 3]`
- Compound: `{key: value}`

### Type assignment
Values can also be assigned a type, which can then be used in all contexts where a type is valid:
```
Person = compound {
    first-name = string
    last-name = string
}
```

### Expressions
Expressions are a way to do some computation in the config file.
```
name: (
    print "Enter your name: "
    readLn
)
_: printLn ("Hello, " name)
```

### Validating types
The type of a value can be validated using the `validate` builtin:
```
foo: {
    bar: 1
    baz: "Hello, World!"
    a-list: [
        "Hello"
        "World"
    ]
}

_: validate foo compound {
    bar = number
    baz = string
    a-list = list[string]
}
```

`validate` has the following syntax:
```
validate <path> (optional) <type-validation>
```

Where:
- `<path>`: The path to the value to validate (example: `foo.bar`)
- `(optional)`: Optional, specifies that any key not specified in the validation will be ignored
- `<type-validation>`: The expected type
  - `compound { <key> = <type-validation> }`: A compound type with the expected keys and types
  - `number`: A number
  - `list[<type-validation>]`: A list of the specified type
  - `string`: A string

`validate` returns a `number` which can be used to check if the validation was successful.

## Usage
To run a Lynx file, you can use the `lynx` command.
```
lynx <file>
```
