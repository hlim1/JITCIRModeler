// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/UInt8Array

function uint8array(x) {
    let y = new Uint8Array(x);
    let z = new Uint8Array(y);
    z[0] = 42;
}

for (let i = 0; i < 10000; i++) {
    uint8array([21,31]);
}
