function arccosine(x) {
    return Math.acos(x);
}

let result;
for (let i = 0; i < 10000; i++) {
    result = arccosine(0.5);
}
