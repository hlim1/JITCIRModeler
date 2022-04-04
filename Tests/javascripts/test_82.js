function imul(x, y) {
    return Math.imul(x, y);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = imul(3, 4);
    result = imul(-3, 12);
    result = imul(0xffffffff, 5);
    result = imul(0xfffffffe, 5);
}
