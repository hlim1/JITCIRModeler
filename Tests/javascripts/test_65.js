// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/BigUint64Array

function biguint64array(x) {
    let arr = new BigUint64Array(x);
    let arr2 = new BigUint64Array(arr);
}

for (let i = 0; i < 10000; i++) {
    biguint64array([21n,31n]);
}
