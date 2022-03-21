function toUnaryPlus(x) {
    return +x;
}

let result;
for (let i = 0; i < 10000; i++) {
    result = toUnaryPlus(-100);
}
