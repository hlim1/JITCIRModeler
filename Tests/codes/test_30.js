function hatan(x) {
    return Math.atanh(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = hatan(1);
}
