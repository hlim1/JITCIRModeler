function addition(x, y) {
    return x + y;
}

let result;
for (let i = 0; i < 10000; i++) {
    result = addition(100, 200);
}
