
#include "global.h"


class SRGObject : public node::ObjectWrap {

public:
	char *seedBuf;
	size_t seedLen;
	struct random_context *entropy;
	static v8::Persistent<v8::Function> constructor;
	Persistent<Function, CopyablePersistentTraits<Function>> *seedCallback;
	Isolate *isolate;
	Persistent<Array> seedArray;
	static PLINKQUEUE signingEntropies;
public:

	static void Init( Isolate *isolate, Handle<Object> exports );
	SRGObject( Persistent<Function, CopyablePersistentTraits<Function>> *callback );
	SRGObject( const char *seed, size_t seedLen );
	SRGObject();

private:
	static void getSeed( uintptr_t psv, POINTER *salt, size_t *salt_size ) {
		SRGObject* obj = (SRGObject*)psv;
		if( obj->seedCallback ) {
			Local<Function> cb = Local<Function>::New( obj->isolate, obj->seedCallback[0] );
			Local<Array> ui = Array::New( obj->isolate );
			Local<Value> argv[] = { ui };
			{
				// if the callback exceptions, this will blindly continue to generate random entropy....
				MaybeLocal<Value> result = cb->Call( obj->isolate->GetCurrentContext()->Global(), 1, argv );
				if( result.IsEmpty() )
					return;
			}
			for( uint32_t n = 0; n < ui->Length(); n++ ) {
				Local<Value> elem = ui->Get( n );
				String::Utf8Value val( USE_ISOLATE( obj->isolate ) elem->ToString());
				obj->seedBuf = (char*)Reallocate( obj->seedBuf, obj->seedLen + val.length() );
				memcpy( obj->seedBuf + obj->seedLen, (*val), val.length() );
			}
		}
		if( obj->seedBuf ) {
			salt[0] = (POINTER)obj->seedBuf;
			salt_size[0] = obj->seedLen;
			Deallocate( char *, obj->seedBuf );
			obj->seedBuf = NULL;
			obj->seedLen = 0;
		}
		else
			salt_size[0] = 0;
	}
	static void New( const v8::FunctionCallbackInfo<Value>& args ) {
		Isolate* isolate = args.GetIsolate();
		if( args.IsConstructCall() ) {
			SRGObject* obj;
			int argc = args.Length();
			if( argc > 0 ) {
				if( args[0]->IsFunction() ) {
					Handle<Function> arg0 = Handle<Function>::Cast( args[0] );
					obj = new SRGObject( new Persistent<Function, CopyablePersistentTraits<Function>>( isolate, arg0 ) );
				}
				else {
					String::Utf8Value seed( USE_ISOLATE( isolate ) args[0]->ToString() );
					obj = new SRGObject( *seed, seed.length() );
				}
				obj->Wrap( args.This() );
				args.GetReturnValue().Set( args.This() );
			}
			else
			{
				obj = new SRGObject();
				obj->Wrap( args.This() );
				args.GetReturnValue().Set( args.This() );
			}
		}
		else {
			// Invoked as plain function `MyObject(...)`, turn into construct call.
			int argc = args.Length();
			Local<Value> *argv = new Local<Value>[argc];
			for( int n = 0; n < argc; n++ )
				argv[n] = args[n];

			Local<Function> cons = Local<Function>::New( isolate, constructor );
			args.GetReturnValue().Set( cons->NewInstance( isolate->GetCurrentContext(), argc, argv ).ToLocalChecked() );
			delete[] argv;
		}
	}
	static void reset( const v8::FunctionCallbackInfo<Value>& args ) {
		SRGObject *obj = ObjectWrap::Unwrap<SRGObject>( args.This() );
		SRG_ResetEntropy( obj->entropy );
	}
	static void seed( const v8::FunctionCallbackInfo<Value>& args ) {
		if( args.Length() > 0 ) {
			String::Utf8Value seed( USE_ISOLATE( args.GetIsolate() ) args[0]->ToString() );
			SRGObject *obj = ObjectWrap::Unwrap<SRGObject>( args.This() );
			if( obj->seedBuf )
				Deallocate( char *, obj->seedBuf );
			obj->seedBuf = StrDup( *seed );
			obj->seedLen = seed.length();
		}
	}
	static void getBits( const v8::FunctionCallbackInfo<Value>& args ) {
		SRGObject *obj = ObjectWrap::Unwrap<SRGObject>( args.This() );
		obj->isolate = args.GetIsolate();
		int32_t r;
		if( !args.Length() )
			r = SRG_GetEntropy( obj->entropy, 32, true );
		else {
			int32_t bits = args[0]->Int32Value( obj->isolate->GetCurrentContext() ).FromMaybe( 0 );
			bool sign = false;
			if( args.Length() > 1 )
				sign = args[0]->BooleanValue();
			r = SRG_GetEntropy( obj->entropy, bits, sign );
		}
		args.GetReturnValue().Set( Integer::New( obj->isolate, r ) );
	}
	static void getBuffer( const v8::FunctionCallbackInfo<Value>& args ) {
		SRGObject *obj = ObjectWrap::Unwrap<SRGObject>( args.This() );
		obj->isolate = args.GetIsolate();
		if( !args.Length() ) {
			obj->isolate->ThrowException( Exception::Error( String::NewFromUtf8( obj->isolate, "required parameter missing, count of bits" ) ) );
		}
		else {
			int32_t bits = args[0]->Int32Value();
			uint32_t *buffer = NewArray( uint32_t, (bits +31)/ 32 );
			SRG_GetEntropyBuffer( obj->entropy, buffer, bits );

			Local<Object> arrayBuffer = ArrayBuffer::New( obj->isolate, buffer, (bits+7)/8 );
			PARRAY_BUFFER_HOLDER holder = GetHolder();
			holder->o.Reset( obj->isolate, arrayBuffer );
			holder->o.SetWeak< ARRAY_BUFFER_HOLDER>( holder, releaseBuffer, WeakCallbackType::kParameter );
			holder->buffer = buffer;

			args.GetReturnValue().Set( arrayBuffer );
		}
	}

	~SRGObject() {
		SRG_DestroyEntropy( &entropy );
	}


	struct bit_count_entry {
		uint8_t ones, in0, in1, out0, out1, changes;
	};
	static struct bit_count_entry bit_counts[256];

	static void InitBitCountLookupTables( void ) {
		int in0 = 0;
		int in1 = 0;
		int out0 = 0;
		int out1 = 0;
		int ones;
		int n;
		for( n = 0; n < 256; n++ ) {
			int changed;
			in0 = 0;
			in1 = 0;
			out0 = 0;
			out1 = 0;
			ones = 0;
			changed = 1;
			if( (n & (1 << 7)) ) ones++;
			if( (n & (1 << 6)) ) ones++;
			if( (n & (1 << 5)) ) ones++;
			if( (n & (1 << 4)) ) ones++;
			if( (n & (1 << 3)) ) ones++;
			if( (n & (1 << 2)) ) ones++;
			if( (n & (1 << 1)) ) ones++;
			if( (n & (1 << 0)) ) ones++;
			do {
				if( (n & (1 << 7)) ) {
					out1++;
					if( (n & (1 << 6)) ) out1++; else break;
					if( (n & (1 << 5)) ) out1++; else break;
					if( (n & (1 << 4)) ) out1++; else break;
					if( (n & (1 << 3)) ) out1++; else break;
					if( (n & (1 << 2)) ) out1++; else break;
					if( (n & (1 << 1)) ) out1++; else break;
					if( (n & (1 << 0)) ) out1++; else break;
					changed = 0;
				}
				else {
					out0++;
					if( !(n & (1 << 6)) ) out0++; else break;
					if( !(n & (1 << 5)) ) out0++; else break;
					if( !(n & (1 << 4)) ) out0++; else break;
					if( !(n & (1 << 3)) ) out0++; else break;
					if( !(n & (1 << 2)) ) out0++; else break;
					if( !(n & (1 << 1)) ) out0++; else break;
					if( !(n & (1 << 0)) ) out0++; else break;
					changed = 0;
				}
			} while( 0 );
			if( !changed ) {
				in1 = out1; in0 = out0;
			} else do {
				if( (n & (1 << 0)) ) {
					in1++;
					if( (n & (1 << 1)) ) in1++; else break;
					if( (n & (1 << 2)) ) in1++; else break;
					if( (n & (1 << 3)) ) in1++; else break;
					if( (n & (1 << 4)) ) in1++; else break;
					if( (n & (1 << 5)) ) in1++; else break;
					if( (n & (1 << 6)) ) in1++; else break;
					if( (n & (1 << 7)) ) in1++; else break;
				}
				else {
					in0++;
					if( !(n & (1 << 1)) ) in0++; else break;
					if( !(n & (1 << 2)) ) in0++; else break;
					if( !(n & (1 << 3)) ) in0++; else break;
					if( !(n & (1 << 4)) ) in0++; else break;
					if( !(n & (1 << 5)) ) in0++; else break;
					if( !(n & (1 << 6)) ) in0++; else break;
					if( !(n & (1 << 7)) ) in0++; else break;
				}
			} while( 0 );
			bit_counts[n].in0 = in0;
			bit_counts[n].in1 = in1;
			bit_counts[n].out0 = out0;
			bit_counts[n].out1 = out1;
			bit_counts[n].changes = changed;
			bit_counts[n].ones = ones;
		}
	}

	static int signCheck( uint8_t *buf, int del1, int del2 ) {
		int n;
		int is0 = bit_counts[buf[0]].in0 != 0;
		int is1 = bit_counts[buf[0]].in1 != 0;
		int long0 = 0;
		int long1 = 0;
		int longest0 = 0;
		int longest1 = 0;
		int ones = 0;
		int rval;
		//LogBinary( buf, 32 );
		for( n = 0; n < 32; n++ ) {
			struct bit_count_entry *e = bit_counts + buf[n];
			ones += e->ones;
			if( is0 && e->in0 ) long0 += e->in0;
			if( is1 && e->in1 ) long1 += e->in1;
			if( e->changes ) {
				if( long0 > longest0 ) longest0 = long0;
				if( long1 > longest1 ) longest1 = long1;
				if( long0 = e->out0 ) {
					is0 = 1;
					is1 = 0;
				} 
				else {
					is1 = 1;
					is0 = 0;
				}
				long1 = e->out1;
			}
			else {
				if( !is0 && e->in0 ) {
					if( long1 > longest1 ) longest1 = long1;
					long0 = e->out0;
					long1 = e->out1;
					is0 = 1;
					is1 = 0;
				}
				else if( !is1 && e->in1 ) {
					if( long0 > longest0 ) longest0 = long0;
					long0 = e->out0;
					long1 = e->out1;
					is0 = 0;
					is1 = 1;
				}
			}
#if 0
			int b;
			for( b = 0; b < 8; b++ ) {
				if( buf[n] & (1 << b) ) {
					ones++;
					if( is1 ) {
						long1++;
					}
					else {
						if( long0 > longest0 ) longest0 = long0;
						is1 = 1;
						is0 = 0;
						long1 = 1;
					}
				}
				else {
					if( is0 ) {
						long0++;
					}
					else {
						if( long1 > longest1 ) longest1 = long1;
						is0 = 1;
						is1 = 0;
						long0 = 1;
					}
				}
			}
#endif
		}
		if( long0 > longest0 ) longest0 = long0;
		if( long1 > longest1 ) longest1 = long1;

// 167-128 = 39 = 40+ dif == 30 bits in a row approx
#define overbal (167-128)
		if( longest0 > (29+del1) || longest1 > (29+del1) || ones > (128+overbal+del2) || ones < (128-overbal-del2) ) {
			if( ones > ( 128 + overbal + del2 ) )
				rval = 1;
			else if( ones < (128 - overbal - del2) )
				rval = 2;
			else if( longest0 > (29+del1 ) )
				rval = 3;
			else if( longest1 > (29+del1 ) )
				rval = 4;
			else
				rval = 5;
			return rval;
		}
		return 0;
	}


	struct signParams {
		PTHREAD main;
		struct random_context *signEntropy;
		POINTER state;
		char *salt;
		int saltLen;
		int tries;
		char *id;
		int pad1;
		int pad2;
		int ended;
		int *done;
	};
	static int signingThreads;

	static uintptr_t signWork( PTHREAD thread ) {
		struct signParams *params = (struct signParams *)GetThreadParam( thread );

		do {
			SRG_RestoreState( params->signEntropy, params->state );
			{
				size_t len;
				uint8_t outbuf[32];
				uint8_t *bytes;
				int passed_as;
				params->id = SRG_ID_Generator_256();
				bytes = DecodeBase64Ex( params->id, 44, &len, (const char*)1 );
				SRG_FeedEntropy( params->signEntropy, bytes, len );
				Release( bytes );
				SRG_GetEntropyBuffer( params->signEntropy, (uint32_t*)outbuf, 256 );
				params->tries++;
				if( (passed_as = signCheck( outbuf, params->pad1, params->pad2 )) ) {
					//lprintf( "FEED" );
					//LogBinary( bytes, len );
					//lprintf( "GOT" );
					//LogBinary( outbuf, 256 / 8 );
					printf( " %d  %s  %d\n", params->tries, params->id, passed_as );
				} 
				else {
					Release( params->id );
					params->id = NULL;
				}
			}
		} while( !params->id && !params->done[0] );
		if( !params->done[0] ) {
			params->done[0] = TRUE;
			WakeThread( params->main );
		}
		params->ended = 1;
		return 0;
	}

	static void addSigningSalt( uintptr_t psv, POINTER *salt, size_t *saltLen ) {
		struct signParams *params = (struct signParams *)psv;
		salt[0] = params->salt;
		saltLen[0] = params->saltLen;
	}

	static void sign( const v8::FunctionCallbackInfo<Value>& args ) {
		Isolate* isolate = args.GetIsolate();
		String::Utf8Value buf( USE_ISOLATE( isolate ) args[0]->ToString() );
		static signParams threadParams[32];
		int found = 0;
		int tries = 0;
		int done = 0;
		int pad1 = 0, pad2 = 0;
		int n = 0;
		int argn = 1;
		int threads = signingThreads;
		POINTER state = NULL;
		while( argn < args.Length() ) {
			if( args[argn]->IsNumber() ) {
				if( n ) {
					pad2 = args[argn]->Int32Value();
				}
				else {
					n = 1;
					pad1 = args[argn]->Int32Value();
				}
			}
			argn++;
		}


		//lprintf( "RESET ENTROPY TO START" );
		//LogBinary( (const uint8_t*)*buf, buf.length() );

		for( n = 0; n < threads; n++ ) {
			threadParams[n].main = MakeThread();
			if( !threadParams[n].signEntropy )
				threadParams[n].signEntropy = SRG_CreateEntropy2_256( NULL, 0 );

			SRG_ResetEntropy( threadParams[n].signEntropy );
			SRG_FeedEntropy( threadParams[n].signEntropy, (const uint8_t*)*buf, buf.length() );
			threadParams[n].state = NULL;
			SRG_SaveState( threadParams[n].signEntropy, &threadParams[n].state );
			threadParams[n].salt = SRG_ID_Generator();
			threadParams[n].saltLen = strlen( threadParams[n].salt );
			threadParams[n].pad1 = pad1;
			threadParams[n].pad2 = pad2;
			threadParams[n].ended = 0;
			threadParams[n].tries = 0;
			threadParams[n].done = &done;
			ThreadTo( signWork, (uintptr_t)(threadParams +n) );
		}

		while( !done ) {
			WakeableSleep( 500 );
		}
		for( n = 0; n < threads; n++ ) {
			while( !threadParams[n].ended ) Relinquish();
			tries += threadParams[n].tries;
		}
		for( n = 0; n < threads; n++ ) {
			if( threadParams[n].id ) {
				if( found ) {
				}
				else {
					lprintf( " %d  %s \n", tries, threadParams[n].id );
					args.GetReturnValue().Set( String::NewFromUtf8( args.GetIsolate(), threadParams[n].id ) );
				}
				Release( threadParams[n].id );
				found++;
			}
			threadParams[n].id = NULL;
			Release( threadParams[n].salt );
			Release( threadParams[n].state );
		}

	}

	static void setThraads( const v8::FunctionCallbackInfo<Value>& args ) {
		if( args.Length() )
			signingThreads = args[0]->Int32Value();
	}


	static void verify( const v8::FunctionCallbackInfo<Value>& args ) {
		Isolate* isolate = args.GetIsolate();
		if( args.Length() > 1 ) {
			String::Utf8Value buf( USE_ISOLATE( isolate ) args[0]->ToString() );
			String::Utf8Value hash( USE_ISOLATE( isolate ) args[1]->ToString() );
			//SRGObject *obj = ObjectWrap::Unwrap<SRGObject>( args.This() );
			char *id;
			int tries = 0;
			int pad1 = 0, pad2 = 0;
			int n = 0;
			int argn = 1;
			struct random_context *signEntropy = (struct random_context *)DequeLink( &signingEntropies );
			while( argn < args.Length() ) {
				if( args[argn]->IsNumber() ) {
					if( n ) {
						pad2 = args[argn]->Int32Value();
					} else {
						n = 1;
						pad1 = args[argn]->Int32Value();
					}
				}
				argn++;
			}

			if( !signEntropy )
				signEntropy = SRG_CreateEntropy2_256( NULL, (uintptr_t)0 );
			SRG_ResetEntropy( signEntropy );
			SRG_FeedEntropy( signEntropy, (const uint8_t*)*buf, buf.length() );
			{
				size_t len;
				uint8_t outbuf[32];
				uint8_t *bytes;
				id = *hash;
				bytes = DecodeBase64Ex( id, 44, &len, (const char*)1 );
				//lprintf( "FEED INIT" );
				//LogBinary( (*buf), buf.length() );
				//lprintf( "FEED" );
				//LogBinary( bytes,len );
				SRG_FeedEntropy( signEntropy, bytes, len );
				Release( bytes );
				SRG_GetEntropyBuffer( signEntropy, (uint32_t*)outbuf, 256 );
				//lprintf( "GET" );
				//LogBinary( outbuf, 256 / 8 );
				args.GetReturnValue().Set( Number::New( args.GetIsolate(), signCheck( outbuf, pad1, pad2 ) ) );
			}
			EnqueLink( &signingEntropies, signEntropy );
		}
	}

};

struct SRGObject::bit_count_entry SRGObject::bit_counts[256];
PLINKQUEUE SRGObject::signingEntropies;
v8::Persistent<v8::Function> SRGObject::constructor;


void InitSRG( Isolate *isolate, Handle<Object> exports ) {
	SRGObject::Init( isolate, exports );
}

void SRGObject::Init( Isolate *isolate, Handle<Object> exports )
{
	InitBitCountLookupTables();
	Local<FunctionTemplate> srgTemplate;
	srgTemplate = FunctionTemplate::New( isolate, New );
	srgTemplate->SetClassName( String::NewFromUtf8( isolate, "sack.core.srg" ) );
	srgTemplate->InstanceTemplate()->SetInternalFieldCount( 1 );  // need 1 implicit constructor for wrap
	NODE_SET_PROTOTYPE_METHOD( srgTemplate, "seed", SRGObject::seed );
	NODE_SET_PROTOTYPE_METHOD( srgTemplate, "reset", SRGObject::reset );
	NODE_SET_PROTOTYPE_METHOD( srgTemplate, "getBits", SRGObject::getBits );
	NODE_SET_PROTOTYPE_METHOD( srgTemplate, "getBuffer", SRGObject::getBuffer );
	Local<Function> f = srgTemplate->GetFunction();
	SRGObject::constructor.Reset( isolate, f );

	SET_READONLY( exports, "SaltyRNG", f );
	SET_READONLY_METHOD( f, "sign", SRGObject::sign );
	SET_READONLY_METHOD( f, "setSigningThreads", SRGObject::setThraads );
	SET_READONLY_METHOD( f, "verify", SRGObject::verify );

}

SRGObject::SRGObject( Persistent<Function, CopyablePersistentTraits<Function>> *callback ) {
	this->seedBuf = NULL;
	this->seedLen = 0;
	this->seedCallback = callback;

	this->entropy = SRG_CreateEntropy2( SRGObject::getSeed, (uintptr_t) this );
}

SRGObject::SRGObject() {
	this->seedBuf = NULL;
	this->seedLen = 0;
	this->seedCallback = NULL;
	this->entropy = SRG_CreateEntropy2( SRGObject::getSeed, (uintptr_t) this );
}

SRGObject::SRGObject( const char *seed, size_t seedLen ) {
	this->seedBuf = StrDup( seed );
	this->seedLen = seedLen;
	this->seedCallback = NULL;
	this->entropy = SRG_CreateEntropy2( SRGObject::getSeed, (uintptr_t) this );
}

