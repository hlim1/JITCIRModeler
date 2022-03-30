// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Int16Array

function int16array(x) {
    let y = new Int16Array(x);
    let z = new Int16Array(y);
    z[0] = 42;
}

for (let i = 0; i < 10000; i++) {
    int16array([21,31]);
}
