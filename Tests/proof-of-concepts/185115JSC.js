function mathRoundOnDoubles(value)
{
    return Math.round(value);
}
noInline(mathRoundOnDoubles);

var x;
for (var i = 0; i < 30000; i++) {
    //print (mathRoundOnDoubles(0.49999999999999994));
    x = mathRoundOnDoubles(0.49999999999999994);
}
