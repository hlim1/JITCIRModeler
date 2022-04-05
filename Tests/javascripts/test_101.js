// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/UInt8Array

function uint8array(x) {
    let uint8 = new Uint8Array(x);
    uint8[0] = 41;
}

for (let i = 0; i < 10000; i++) {
    uint8array([21,31]);
}
