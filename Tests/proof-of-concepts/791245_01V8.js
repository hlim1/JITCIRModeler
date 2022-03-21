var a, b;  // should be var
for (var i = 0; i < 100000; i++) {
    b = 1;
    a = i + -0;  // -0 is a number, so this will make "a" a heap object.
    b = a;
}

print(a === b);  // true
gc();
print(a === b);  // false
print(b);
