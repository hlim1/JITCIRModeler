function assignments(x, y) {
    
    let z = 0;

    z = x;
    z += x;
    z -= x;
    z *= y;
    z ^= y;
    z /= y;
    z %= y;

    return z;
}

let result;
for (let i = 0; i < 10000; i++) {
    result = assignments(100, 2);
    print(result);
}
