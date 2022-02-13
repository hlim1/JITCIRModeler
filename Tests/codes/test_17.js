function comparisonLessThan(x, y) {
    
    let z = 100;
    if (x < y) {
        z++;
    }
    else {
        z--;
    }
    return z;
}

let result;
for (let i = 0; i < 10000; i++) {
    result = comparisonLessThan(100, 200);
}
