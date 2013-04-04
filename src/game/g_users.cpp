#include <vector>
#include <string>

#include "g_local.hpp"
#include "g_utilities.h"
#include "g_users.h"

const char CREATE_USERS_TABLE[] =
    "CREATE TABLE IF NOT EXISTS Users("
    "UserID INTEGER PRIMARY KEY,"
    "Title VARCHAR(256),"
    "Commands VARCHAR(256),"
    "Greeting VARCHAR(256),"
    "Guid VARCHAR(40) UNIQUE,"
    "HardwareID VARCHAR(40),"
    "IP VARCHAR(16),"
    "Level INTEGER);";
const char CREATE_ALIAS_TABLE[] =
    "CREATE TABLE IF NOT EXISTS Aliases(AliasID INTEGER PRIMARY KEY,\
    Alias VARCHAR(36), UserID INTEGER, FOREIGN KEY(UserID) REFERENCES\
    Users(UserID));";

const char INVALID_GUID_FRAME[] = 
    "Error: invalid guid info frame from %s: %s\n";
const char INVALID_LOGIN_FRAME[] = 
"Error: invalid login info frame from %s: %s\n";
const char INVALID_IP[] =
    "Error: invalid IP %s";
const char INVALID_HWID_FRAME[] = 
    "Error: invalid HWID frame from %s: %s\n";

void ClientGuidReceived(gentity_t *ent) {
    Arguments argv = GetArgs();


    // etguid <GUID>
    if( argv->size() != 2 ) {
        G_LogPrintf(INVALID_GUID_FRAME, 
            ent->client->pers.netname, ConcatArgs(0));
        return;
    }

    if(!users.SetGuid(ent, argv->at(1))) {
        G_LogPrintf(INVALID_GUID_FRAME,
            ent->client->pers.netname, ConcatArgs(0));
        return;
    }


}

void ClientCredentialsReceived(gentity_t *ent) {
    Arguments argv = GetArgs();

    // login username password
    if( argv->size() != 3 ) {
        G_LogPrintf(INVALID_LOGIN_FRAME, 
            ent->client->pers.netname, ConcatArgs(0));
        return;
    }

    if(!users.SetUsername(ent, argv->at(1))) {
        G_LogPrintf(INVALID_LOGIN_FRAME,
            ent->client->pers.netname, ConcatArgs(0));
        return;
    }

    if(!users.SetPassword(ent, argv->at(2))) {
        G_LogPrintf(INVALID_LOGIN_FRAME,
            ent->client->pers.netname, ConcatArgs(0));
        // Reset username because password was invalid
        users.ResetUsername(ent);
        return;
    }
}

void ClientHWIDReceived(gentity_t *ent) {
    Arguments argv = GetArgs();

    // HWID <ID>
    if( argv->size() != 2 ) {
        G_LogPrintf(INVALID_LOGIN_FRAME, ent->client->pers.netname,
            ConcatArgs(0));
        return;
    }

    if( !users.SetHardwareID(ent, argv->at(1))) {
        G_LogPrintf(INVALID_LOGIN_FRAME, ent->client->pers.netname,
            ConcatArgs(0));
        return;
    }
}

void UserDatabase_SetIP(gentity_t *ent, const char* ip) 
{
    users.SetIP(ent, ip);
}

void UserDatabase_Print( gentity_t *caller ) {
    Arguments argv = GetArgs();
    int clientNum = -1;

    if( argv->size() != 2 ) {
        ConsolePrintTo(caller, "usage: udbprint <clientNum>");
        return;
    }

    if(!StringToInt(argv->at(1), clientNum)) {
        ConsolePrintTo(caller, "udbprint: invalid number specified.");
        return;
    }

    users.Print(caller, clientNum);
}

std::string UserDatabase_Guid(gentity_t *ent) {
    return users.Guid(ent);
}

/*
 * UserDB class
 */

UserDatabase::UserDatabase()
{
    
}

UserDatabase::~UserDatabase()
{
    
}

UserDatabase::Client::Client()
{
    guid.clear();
    hardwareID.clear();
    ip.clear();
    password.clear();
    username.clear();
    dbUserID = -1;
    level = 0;
}

bool UserDatabase::SetGuid( gentity_t *ent, const std::string& guid )
{
    if( guid.length() != GUID_LEN ) {
        return false;
    }

    clients_[ent->client->ps.clientNum].guid = guid;

    if( !g_admin.integer ) {
        return true;
    }

    try {

        sqlite3pp::command cmd(db_, "INSERT OR IGNORE INTO Users (Guid, Level) VALUES (?, '0');");
        cmd.bind(1, guid.c_str());
        cmd.execute();
        
        sqlite3pp::query query(db_, "SELECT UserID, Level FROM Users Where Guid=?1;");
        query.bind(1, guid.c_str());

        sqlite3pp::query::iterator it = query.begin();
        if(it == query.end()) {
            G_LogPrintf("Database error: failed to cache UserID\n");
            // Returns true to save the guid to memory anyway
            return true;
        }
        clients_[ent->client->ps.clientNum].dbUserID =
            (*it).get<int>(0);
        clients_[ent->client->ps.clientNum].level =
            it->get<int>(1);
        
        clients_[ent->client->ps.clientNum].permissions =
            levels.Permissions(clients_[ent->client->ps.clientNum].level);

        // Add the first nick to aliasDB aswell
        AddNameToDatabase(ent);
    } catch ( sqlite3pp::database_error&e) {
        G_LogPrintf("Database error: %s\n", e.what());
    }

    return true;
}

bool UserDatabase::SetHardwareID( gentity_t *ent, const std::string& hardwareID )
{
    if( hardwareID.length() != HARDWARE_ID_LEN ) {
        return false;
    }

    clients_[ent->client->ps.clientNum].hardwareID = hardwareID;

    if( !g_admin.integer ) {
        return true;
    }

    try {
        sqlite3pp::command cmd(db_, "UPDATE Users SET HardwareID=?1 WHERE UserID=?2;");
        cmd.bind(1, hardwareID.c_str());
        cmd.bind(2, clients_[ent->client->ps.clientNum].dbUserID);
        cmd.execute();
    } catch ( sqlite3pp::database_error&e) {
        G_LogPrintf("SQLite3 error: %s\n", e.what());
    }
    return true;
}

bool UserDatabase::SetIP( gentity_t *ent, const std::string& ip )
{
    if( ip.length() < 0 || ip.length() > MAX_IP_LEN ) {
        G_LogPrintf(INVALID_IP, ip.c_str());
        return false;
    } 

    clients_[ent->client->ps.clientNum].ip = ip;

    if( !g_admin.integer ) {
        return true;
    }

    try {
        sqlite3pp::command cmd(db_, "UPDATE Users SET IP=?1 WHERE UserID=?2;");
        cmd.bind(1, ip.c_str());
        cmd.bind(2, clients_[ent->client->ps.clientNum].dbUserID);
        cmd.execute();
    } catch ( sqlite3pp::database_error&e) {
        G_LogPrintf("SQLite3 error: %s\n", e.what());
    }
    return true;
}

bool UserDatabase::SetPassword( gentity_t *ent, const std::string& password )
{
    if( password.length() != 40 ) {
        return false;
    }
    clients_[ent->client->ps.clientNum].password = password;
    return true;
}

bool UserDatabase::SetUsername( gentity_t *ent, const std::string& username )
{
    if( username.length() < 0 ) {
        return false;
    }
    clients_[ent->client->ps.clientNum].username = username;
    return true;
}

std::string UserDatabase::Guid( gentity_t *ent ) const
{
    return clients_[ent->client->ps.clientNum].guid;
}

std::string UserDatabase::HardwareID( gentity_t *ent ) const
{
    return clients_[ent->client->ps.clientNum].hardwareID;
}

std::string UserDatabase::IP( gentity_t *ent ) const
{
    return clients_[ent->client->ps.clientNum].ip;
}

std::string UserDatabase::Password( gentity_t *ent ) const
{
    return clients_[ent->client->ps.clientNum].password;
}

std::string UserDatabase::Username( gentity_t *ent ) const
{
    return clients_[ent->client->ps.clientNum].username;
}

void UserDatabase::Print( gentity_t *caller, int targetClientNum ) const
{
    // Client: client's name
    // { username, password, guid, hardwareID, ip }
    if( targetClientNum < 0 || targetClientNum >= MAX_CLIENTS ) {
        ConsolePrintTo(caller, "udbprint: invalid clientNum");
        return;
    }

    BeginBufferPrint();
    BufferPrint(caller, "Client: ");
    BufferPrint(caller, IntToString(targetClientNum));
    BufferPrint(caller, "\n{ USER: ");
    BufferPrint(caller, clients_[targetClientNum].username + ", PASSWORD: ");
    BufferPrint(caller, clients_[targetClientNum].password + ", GUID: ");
    BufferPrint(caller, clients_[targetClientNum].guid + ", HARDWAREID: ");
    BufferPrint(caller, clients_[targetClientNum].hardwareID + ", IP: ");
    BufferPrint(caller, clients_[targetClientNum].ip + ", USERID: ");
    BufferPrint(caller, IntToString(clients_[targetClientNum].dbUserID));
    BufferPrint(caller, clients_[targetClientNum].permissions.to_string() + " }");    
    FinishBufferPrint(caller, true);
}

void UserDatabase::ResetGuid( gentity_t *ent )
{
    clients_[ent->client->ps.clientNum].guid.clear();
}

void UserDatabase::ResetHardwareID( gentity_t *ent )
{
    clients_[ent->client->ps.clientNum].hardwareID.clear();
}

void UserDatabase::ResetIP( gentity_t *ent )
{
    clients_[ent->client->ps.clientNum].ip.clear();
}

void UserDatabase::ResetPassword( gentity_t *ent )
{
    clients_[ent->client->ps.clientNum].password.clear();
}

void UserDatabase::ResetUsername( gentity_t *ent )
{
    clients_[ent->client->ps.clientNum].username.clear();
}

void UserDatabase::ResetDBUserID( gentity_t *ent )
{
    clients_[ent->client->ps.clientNum].dbUserID = -1;
}

void UserDatabase::ResetData( gentity_t *ent )
{
    ResetGuid(ent);
    ResetHardwareID(ent);
    ResetIP(ent);
    ResetPassword(ent);
    ResetUsername(ent);
    ResetDBUserID(ent);
    ResetCommands(ent);
    ResetTitle(ent);
    ResetPermissions(ent);
    ResetLevel(ent);
    ResetGreeting(ent);
}

void UserDatabase_AddNameToDatabase( gentity_t *ent ) {
    users.AddNameToDatabase(ent);
}

void UserDatabase::AddNameToDatabase( gentity_t *ent )
{
    // if neither is not set, database won't be open
    // so don't do anything
    if( !g_admin.integer || !g_aliasDB.integer ) {
        return;
    }

    // insert into aliases if certain userid does not have the name already
    // FIXME: create a proper SQL sentence for this. I think this is way 
    // too complex
    try {
        sqlite3pp::command cmd(db_,
            "INSERT INTO Aliases (Alias, UserID) "
                "SELECT * "
                "FROM (SELECT ?1, ?2) AS temp "
                "WHERE NOT EXISTS "
                    "(SELECT Alias "
                    "FROM Aliases "
                    "WHERE Alias=?1 AND UserID=?2) "
                    "LIMIT(1);");
        cmd.bind(1, ent->client->pers.netname);
        cmd.bind(2, clients_[ent->client->ps.clientNum].dbUserID);
        cmd.execute();
    } catch ( sqlite3pp::database_error& e ) {
        G_LogPrintf("SQLite3 Error: %s\n", e.what());
    }
}

const std::vector<std::string> * UserDatabase::GetAliases( gentity_t *ent )
{
    // if either is not set return nothing as access to db is not
    // initialized (most likely)
    if( !g_admin.integer || !g_aliasDB.integer ) {
        return NULL;
    }

    static std::vector<std::string> aliases;
    // clear aliases on every function execution, else we would have
    // all aliases ever requested
    aliases.clear();
    try {
        sqlite3pp::query query(db_, "SELECT Alias "
                                    "FROM Aliases "
                                    "WHERE UserID=?1;");
        query.bind(1, clients_[ent->client->ps.clientNum].dbUserID);

        for(sqlite3pp::query::iterator it = query.begin(); 
            it != query.end(); it++) 
        {
            const char *alias = (*it).get<const char*>(0);

            if(alias) {
                aliases.push_back(alias);
            } 
        }
    } catch ( sqlite3pp::database_error & e) {
        G_LogPrintf("SQLite3 Error: %s\n", e.what());
        return NULL;
    }
    return &aliases;
}

void UserDatabase::Init()
{
    if( !g_admin.integer ) {
        return;
    }
    try {
        // TODO: paths
        db_.connect("etjump/etjump.db");
        db_.execute(CREATE_USERS_TABLE);
        db_.execute(CREATE_ALIAS_TABLE);
    } catch ( sqlite3pp::database_error& e ) {
        G_LogPrintf("User database error: %s\nRestart map to try again", 
            e.what());
    }
}

void UserDatabase::Shutdown()
{
    if( !g_aliasDB.integer ) {
        return;
    }
    db_.disconnect();
}

std::bitset<MAX_CMDS> UserDatabase::Permissions( gentity_t *ent ) const
{
    return clients_[ent->client->ps.clientNum].permissions;
}

int UserDatabase::Level( gentity_t *ent ) const
{
    return clients_[ent->client->ps.clientNum].level;
}

bool UserDatabase::SetLevel( gentity_t *ent, int level )
{
    // Update level
    clients_[ent->client->ps.clientNum].level = level;
    // Update permissions
    clients_[ent->client->ps.clientNum].permissions =
        levels.Permissions(level);
    // Update database
    try {
        sqlite3pp::command cmd(db_, "UPDATE Users SET Level=?1 WHERE UserID=?2;" );
        cmd.bind(1, level);
        cmd.bind(2, clients_[ent->client->ps.clientNum].dbUserID);
        cmd.execute();
    } catch ( sqlite3pp::database_error & e) {
        G_LogPrintf("SQLite3 Error: %s\n", e.what());
        return false;
    }
    return true;
}

void UserDatabase::ResetTitle( gentity_t *ent )
{
    clients_[ent->client->ps.clientNum].title.clear();
}

void UserDatabase::ResetCommands( gentity_t *ent )
{
    clients_[ent->client->ps.clientNum].commands.clear();
}

void UserDatabase::ResetGreeting( gentity_t *ent )
{
    clients_[ent->client->ps.clientNum].greeting.clear();
}

void UserDatabase::ResetLevel( gentity_t *ent )
{
    clients_[ent->client->ps.clientNum].level = 0;
}

void UserDatabase::ResetPermissions( gentity_t *ent )
{
    clients_[ent->client->ps.clientNum].permissions.reset();
}















