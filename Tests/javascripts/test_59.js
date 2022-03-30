// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Bigint64Array

function bigint64array(x) {
    let arr = new BigInt64Array(x);
    arr[0] = 41n;
}

for (let i = 0; i < 10000; i++) {
    bigint64array([21n,31n]);
}
