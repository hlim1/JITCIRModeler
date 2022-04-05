// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/UInt32Array

function uint32array(x) {
    let uint32 = new Uint32Array(x);
    uint32[0] = 42;
}

for (let i = 0; i < 10000; i++) {
    uint32array(2);
}
