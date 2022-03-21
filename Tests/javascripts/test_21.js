function logicalAND(x, y) {
    
    let z = 99;
    if (x == y && x > z) {
        z++;
    }

    return z;
}

let result;
for (let i = 0; i < 10000; i++) {
    result = logicalAND(100, 100);
}
