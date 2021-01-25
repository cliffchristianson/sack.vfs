

import {sack} from "../../../vfs_module.mjs"
import {BloomNHash} from "./bloomNHash.mjs"

const l = {
	ids : {
		accountId : null,
		emailId : null,
		reconnectId : null
	},
	account   : null,
	email     : null,
	reconnect : null
};

let inited = false;
let initResolve = null;
let initializing = new Promise( (res,rej)=>{
	initResolve = res;
} ).then( ()=>{
	inited = true;
} );


const storage = sack.ObjectStorage( "data.os" );
const JSOX = sack.JSOX;
console.log( "test" );
BloomNHash.hook( storage );

storage.getRoot().then( (root)=>{
	root.open( "config.jsox" ).then( (file)=>{
		return file.read().then( (obj)=>{
			console.log( "GOT:", obj );
			Object.assign( l.ids, obj );
		
      return storage.get( l.ids.emailId ).then( hash=>{
			l.email = hash;

	      return storage.get( l.ids.accountId ).then( hash=>{
				l.account = hash;

		      return storage.get( l.ids.reconnectId ).then( hash=>{
					l.reconnect = hash;
				console.log( "GO (reload)" );
					initResolve();
				} );
			} );
		} );
		} );
	} ).catch( (err)=>{
		console.log( "Err:", err );
		root.create( "config.jsox" ).then( (file)=>{
			console.log("create file:", file );
			l.account = new BloomNHash();
			l.email = new BloomNHash();
			l.reconnect = new BloomNHash();

			l.account.store().then((id)=>{
				l.ids.accountId = id;
				return l.email.store().then((id)=>{
					l.ids.emailId = id;
					return l.reconnect.store().then((id)=>{
						l.ids.reconnectId = id;
						file.write( l.ids );
						initResolve();
				console.log( "GO (init)" );
					} );
				} );
			} );
		} );
	} );
} );

class UniqueIdentifier {
	key = null;
	#id = null;
	get id() { return this.#id } ;
	constructor() {
	}
	create( account,user,email,pass ){
		const newUser = new User();
		newUser.account = account;
		newUser.user = user;
		newUser.email = email;
		newUser.pass = pass;
		newUser.unique = this;
		return newUser;
	}
	store() {
		if( !this.#id ) {
			// might have been reloaded...
			const container = storage.getContainer( this );
			if( container ) this.#id = container.id;
			return storage.put( this, {id:this.#id} ).then( (id)=>{
				if( !this.#id ) this.#id = id;
				else if( this.#id !== id ) { console.log( "Object has been duplicated: old/new id:", this.#id, id ); }
			} );
		} else {
			return storage.put( this, {id:this.#id} ).then( (id)=>{
				if( this.#id !== id ) { console.log( "Object has been duplicated: old/new id:", this.#id, id ); }
			} );
		}
	}
}

class User {
	unique = null;
	account = null;
	name = null;
	email = null;
	password = null;
	devices = [];
	#id = null;
	
	constructor() {
	}
	store() {
		if( !this.#id ) {
			// might have been reloaded...
			const container = storage.getContainer( this );
			if( container ) this.#id = container.id;
			return storage.put( this, {id:this.#id} ).then( (id)=>{
				if( !this.#id ) this.#id = id;
				else if( this.#id !== id ) { console.log( "Object has been duplicated: old/new id:", this.#id, id ); }
				l.account.set( this.account, this );
				l.email.set( this.email, this );
			} );
		} else {
			return storage.put( this, {id:this.#id} ).then( (id)=>{
				if( this.#id !== id ) { console.log( "Object has been duplicated: old/new id:", this.#id, id ); }
				l.account.set( this.account, this );
				l.email.set( this.email, this );
			} );
		}


	}
	//create( account,user,email,pass ) {
	//}
}

User.get = function( account ) {
	if( !inited ) {
		return initializing.then( ()=>{
			return l.account.get( account );
		} );
	}
	return l.account.get( account );
}

User.getEmail = function( email ) {
	if( !inited ) {
		return initializing.then( ()=>{
			return l.email.get( email );
		} );
	}
	return l.email.get( email );
}

class Device {
	key = null;
	active = false;
	#id = null;
	get id() { return this.#id } ;
	constructor() {
	}
	store() {
		if( !this.#id ) {
			// might have been reloaded...
			const container = storage.getContainer( this );
			if( container ) this.#id = container.id;
			return storage.put( this, {id:this.#id} ).then( (id)=>{
				if( !this.#id ) this.#id = id;
				else if( this.#id !== id ) { console.log( "Object has been duplicated: old/new id:", this.#id, id ); }
			} );
		} else {
			return storage.put( this, {id:this.#id} ).then( (id)=>{
				if( this.#id !== id ) { console.log( "Object has been duplicated: old/new id:", this.#id, id ); }
			} );
		}
	}		
}


export {User,Device,UniqueIdentifier,initializing as go} ;