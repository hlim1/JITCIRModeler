// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/test

const str1 = 'table football';
const regex = new RegExp('foo*');
let result;

function test(str) {
    return regex.test(str);
}

for (let i = 0; i < 10000; i++) {
    result = test(str1)
}
