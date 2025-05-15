# Lynx Config Language
Its just a config file bro...
Well, it can do a bit more than that... like [FizzBuzz](./examples/fizzbuzz.lynx)!

## Syntax
The syntax is pretty simple, it is a key-value pair separated by a colon. The key is the name of the variable and the value is the value of the variable. The value can be a string, number, boolean, or an array, or an expression.

### Example
```
-- This is a comment
foo = {
    bar = 1
    baz = "Hello, World!"
    a-list = [
        "Hello"
        "World"
    ]
}

-- Concatenating multiple strings
a-string = (
    "Hello, "
    "World!"
)
```
Any values contained in `()` get concatenated into a single value. All values must be the same type, with the exception of numbers, which can convert into strings.

### Types
- String: `"Hello, World!"`
- Number: `1`/`2.34`/`true`/`false`
  - Booleans are considered numbers
- List: `[1 2 3]`
- Compound: `{key: value}`

### Type assignment
Values can also be assigned a type, which can then be used in all contexts where a type is valid:
```
Person: compound {
    first-name: string
    last-name: string
}
```

### Expressions
Expressions are a way to do some computation in the config file.
```
name = (
    print "Enter your name: "
    readLn
)

(printLn ("Hello, " name))
```

### Generating values
You can generate values conditionally using the `if` statement or with looping patterns using the `for` statement:
```
a: 1
b: if eq a 1 (
    2
) else (
    3
)

(for i in [1 2 3] (
    printLn i
))
```

You can also use the `for` statement to iterate over a previously defined list:
```
a-list = [
    "Hello"
    "World"
]

(for i in a-list (
    printLn i
))
```

You can also use the `switch` statement to generate values based on the value of a variable:
```
x = switch (os-name) (
    "Windows" = "Windows"
    "Linux" = "Linux"
    "Mac OS X" = "macOS"
    else = "Unknown/Other OS"
)

(printLn ("OS: " x))
```

### Validating types
The type of a value will automatically validated, if that value had a type assigned to it.
```
-- Type:
foo: compound {
    bar: number
    baz: string
    a-list: list[string]
}

-- Data:
foo = {
    bar = 1
    baz = "Hello, World!"
    a-list = [
        "Hello"
        "World"
    ]
}
```
If the type of a value does not match the type assigned to it, an error will be thrown.

### Functions
Functions are a way to define reusable code. They can take arguments and return values.
```
-- Function definition
printTwice = func(s: string) (
    printLn s
    printLn s
)

(printTwice "Hello")
```

### Built-in functions
- `true`: Returns true (1)
- `false`: Returns false (0)
- `runshell (cmd: string)`: Runs a shell command and returns the output
- `print (s: string)`: Prints a string to the console
- `printLn (s: string)`: Prints a string to the console and adds a new line
- `readLn`: Reads a line from the console
- `use (file: string)`: Merges another file into the current file
- `eq (a: any, b: any)`: Returns true if a and b are equal
- `ne (a: any, b: any)`: Returns true if a and b are not equal
- `string-length (s: string)`: Returns the length of a string
- `string-substring (s: string, start: number, end: number)`: Returns a substring of a string
- `exists (path: path)`: Returns true if the given path exists in the config
- `lt (a: number, b: number)`: Returns true if a is less than b
- `le (a: number, b: number)`: Returns true if a is less than or equal to b
- `gt (a: number, b: number)`: Returns true if a is greater than b
- `ge (a: number, b: number)`: Returns true if a is greater than or equal to b
- `add (a: number, b: number)`: Returns the sum of a and b
- `sub (a: number, b: number)`: Returns the difference of a and b
- `inc (a: number)`: Increments a by 1
- `dec (a: number)`: Decrements a by 1
- `mul (a: number, b: number)`: Returns the product of a and b
- `div (a: number, b: number)`: Returns the quotient of a and b
- `mod (a: number, b: number)`: Returns the remainder of a and b
- `and (a: any, b: any)`: Returns true if a and b are both true
- `or (a: any, b: any)`: Returns true if a or b is true
- `not (a: any)`: Returns true if a is false
- `shl (a: number, b: number)`: Returns a left shift of a by b bits
- `shr (a: number, b: number)`: Returns a right shift of a by b bits
- `range (start: number, end: number)`: Returns a list of numbers from start to end
- `len (list: list[any])`: Returns the length of a list
- `list-get (list: list[any], index: number)`: Returns the value at the given index in the list
- `list-set (list: list[any], index: number, value: any)`: Sets the value at the given index in the list
- `list-append (list: list[any], value: any)`: Adds a value to the end of the list
- `list-remove (list: list[any], index: number)`: Removes the value at the given index in the list
- `exit (code: number)`: Exits the program with the given code
- `ignore (_: any)`: Ignores the value and returns nothing
- `os-name ()`: Returns the name of the operating system
- `os-arch ()`: Returns the architecture of the operating system
