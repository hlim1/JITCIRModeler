function foo($a,$b) {
    $a = $a|0;
    $b = $b|0;
    var $sub = $a - $b;
    return ($sub|0) < 0;
}

for (var i = 0; i < 10000; i++) print(foo(0x7fffffff,-1));
