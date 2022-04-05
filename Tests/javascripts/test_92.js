// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/UInt16Array

function uint16array(x) {
    let y = new Uint16Array(x);
    let z = new Uint16Array(y);
    z[0] = 42;
}

for (let i = 0; i < 10000; i++) {
    uint16array([21,31]);
}
