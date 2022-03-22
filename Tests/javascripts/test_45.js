// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/exec

const regex1 = RegExp('foo*', 'g');
const str1 = 'table football, foosball';
let array1;

function exec(str) {
    return regex1.exec(str)
}

for (let i = 0; i < 20000; i++) {
    array1 = exec(str1);
}
