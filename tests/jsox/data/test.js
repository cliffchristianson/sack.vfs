const sack = require( "../../.." );
const JSOX = sack.JSOX;

var FS = require('fs');

// Modeled off of (v0.6.18 link; check latest too):
// https://github.com/joyent/node/blob/v0.6.18/lib/module.js#L468-L472
require.extensions['.jsox'] = function (module, filename) {
    var content = FS.readFileSync(filename, 'utf8');
    module.exports = JSOX.parse(content);
};

for( var n = 2; n < process.argv.length; n++ ) {
	var object = require( process.argv[n] );
	console.log( "OUT:", process.argv[n], object );
	var s;
	console.log( "ENC:", s = JSOX.stringify( object, null, "\t" ) );
	var t = JSOX.stringify( JSOX.parse(s), null, "\t" );
	if( s != t ) {
		console.log( "FAIL:", JSOX.stringify( JSOX.parse(s), null, "\t" ) );
	}
}
