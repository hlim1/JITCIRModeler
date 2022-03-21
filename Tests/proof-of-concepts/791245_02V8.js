let arr = [{}];
var v;  // should be var
for (var i = 0; i < 700000; i++) {
    arr[0] = 1;
    v = i + -0;
    arr[0] = v;
}

print(arr[0] === v)  // true
gc();
print(arr[0] === v)  // false
print(arr[0]);
