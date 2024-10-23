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

## Usage
To run a Lynx file, you can use the `lynx` command.
```
lynx <file>
```
