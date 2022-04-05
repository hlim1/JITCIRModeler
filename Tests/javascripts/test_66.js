// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/BigiUint64Array

function biguint64array(x) {
    let buffer = new ArrayBuffer(x);
    let arr = new BigUint64Array(buffer, 0, 4);
}

for (let i = 0; i < 10000; i++) {
    biguint64array(32);
}
