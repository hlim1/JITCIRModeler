function atan2(x, y) {
    return Math.atan2(x, y);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = atan2(5, 5);
}
