

import {BloomNHash} from "./bloomNHash.mjs"
import {sack} from "sack.vfs"
const StoredObject = sack.ObjectStorage.StoredObject;
//import {StoredObject} from "../commonDb.mjs"

const configObject = {
		accountId : null,
		emailId : null,
		reconnectId : null,
	}

const l = {
	ids : configObject,
	account   : null,
	email     : null,
	reconnect : null
};



class UniqueIdentifier extends StoredObject {
	key = null;
	constructor() {
		super();
	}
	create( account,user,email,pass ){
		const newUser = new User();
		newUser.hook( this );
		newUser.account = account;
		newUser.name = user;
		newUser.email = email;
		newUser.pass = pass;
		newUser.unique = this;
		return newUser;
	}
}

class User  extends StoredObject{
	unique = null;
	account = null;
	name = null;
	email = null;
	pass = null;
	devices = [];
	
	constructor() {
		super();
	}
	store() {
		return super.store().then( async (id)=>{	
			//console.log( "what about?", id, l );
			await l.account.set( this.account, this );
			//console.log( "Account was set" );
			await l.email.set( this.email, this );
			//console.log( "email was indexed" );
			return this;
		} );
	}
	addDevice( id, active ) {
		const device = new Device();
		device.hook( this )
		device.key = id;
		device.active = active;
		return device.store().then( ()=>
			this.devices.push(device ) );
	}
}

User.get = function( account ) {
	//console.log( "l?", l );
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

class Device  extends StoredObject{
	key = null;
	active = false;
	constructor() {
		super();
	}
}

let getUser = null;
let getIdentifier = null;

const UserDb = {
	async hook( storage ) {
		BloomNHash.hook( storage );
		storage.addEncoders( [ { tag:"~U", p:User, f: null },  { tag:"~D", p:Device, f: null },  { tag:"~I", p:UniqueIdentifier, f: null } ] );
		storage.addDecoders( [ { tag:"~U", p:User, f: null },  { tag:"~D", p:Device, f: null },  { tag:"~I", p:UniqueIdentifier, f: null } ] );

		getUser = (id)=>{
			return User.get( id );
		};
		getIdentifier = ()=>{
			const unique = new UniqueIdentifier();
			unique.hook( storage );
			return unique;
		}
		const root = await storage.getRoot();
		try {
			const file = await root.open( "userdb.config.jsox" )
		
				const obj = await file.read()
				Object.assign( l.ids, obj );
				l.email     = await storage.get( l.ids.emailId );
				l.account   = await storage.get( l.ids.accountId );
				l.reconnect = await storage.get( l.ids.reconnectId );
			} catch( err){
				console.log( "User Db Config ERR:", err );
				const file = await root.create( "userdb.config.jsox" );
				
					l.account   = new BloomNHash();
					l.account.hook( storage );
					l.email     = new BloomNHash();
					l.email.hook( storage );
					l.reconnect = new BloomNHash();
					l.reconnect.hook( storage );

					l.ids.accountId   = await l.account.store();
					l.ids.emailId     = await l.email.store();
					l.ids.reconnectId = await l.reconnect.store();

					file.write( l.ids );
			} 
	},
	getUser(args){
		return getUser(args);
	},
	User:User,
	getIdentifier() {
		return getIdentifier();
	},
	Device:Device,
	UniqueIdentifier:UniqueIdentifier,
}

Object.freeze( UserDb );
export {configObject as config};
export { UserDb } ;
