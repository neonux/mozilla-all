f = function() {
	var q = 1;
	//for (var j = 0; j < 500; ++j)
	for (var i = 0; i < 5000; ++i)
		q += 2.5;
	print("q=" + q + " i=" + i);
}

var before = Date.now();
f();
var after = Date.now();
print(after - before);
