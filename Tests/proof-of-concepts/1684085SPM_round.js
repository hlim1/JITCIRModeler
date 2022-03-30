with ({}) {}

function round(a, b) {
        return Math.round(a / b) | 0;
}

for (var i = 0; i < 50; i++) {
        round(5,5);
}

// Expected Output: 2
print(round(5,3));
