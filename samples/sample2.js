dumpdata=function(data){
	if(Array.isArray(data)) {
        return sprintf("[ %s ]", data.map(function(e){return dumpdata(e);}).join(", "));

    } else if (typeof(data)=="object") {
        return sprintf("{ %s }", Object.keys(data).map(function(k){
                return sprintf("%s: %s", k, dumpdata(data[k]));
        	}).join(", "));

    } else {
        return data;
    }
}

println(dumpdata(v));
println(dumpdata(l));
println('These may not be in order');
println(dumpdata(m));
println(dumpdata(m2));
println(dumpdata(d));
println(dumpdata(mm));
println(dumpdata(a));
println(dumpdata(composite));
