function comparisonEqual(x, y) {
    
    let z = 100;
    if (x == y) {
        z++;
    }
    else {
        z--;
    }
    return z;
}

let result;
for (let i = 0; i < 10000; i++) {
    result = comparisonEqual(100, 100);
}
