// Reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/search

const regexp = /t(e)(st(\d?))/g;
const str1 = 'test1test2';

let array = []

function matchAll(str) {
    return [...str.matchAll(regexp)];
}

for (let i = 0; i < 10000; i++) {
    array = matchAll(str1);
}
