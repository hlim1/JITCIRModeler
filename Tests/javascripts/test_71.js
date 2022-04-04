function fround(x) {
    return Math.fround(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = fround(3.14);
}
