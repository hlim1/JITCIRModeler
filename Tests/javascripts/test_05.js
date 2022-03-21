function mod(x, y) {
    return x % y;
}

let result;
for (let i = 0; i < 10000; i++) {
    result = mod(100, 200);
}
