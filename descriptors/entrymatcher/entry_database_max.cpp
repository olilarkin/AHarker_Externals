
#include "entry_database_max.h"
#include <algorithm>

typedef struct entry_database{
    
    t_object a_obj;
    
    EntryDatabase *database;
    
    long count;
    bool notify;
    
} t_entry_database;

// FIX - this needs to use notification... also needs to make the patcher dirty - best done in the max object part...

void modified(t_entry_database *x, t_symbol *msg, long argc, t_atom *argv)
{
    x->notify = true;
    object_notify(atom_getobj(argv), _sym_modified, NULL);
}

NotifyPointer::~NotifyPointer()
{
    atom a;
    atom_setobj(&a, mClient);
    
    t_entry_database *database = (t_entry_database *) mMaxDatabase;
    
    if (database->notify)
    {
        database->notify = false;
        defer_low(mMaxDatabase, (method) modified, NULL, 1, &a);
    }
}

t_class *database_class;
const char database_class_name[] = "__entry_database";
t_symbol *name_space_name = gensym("__entry_database_private");

int entry_database_init();
void *entry_database_new(t_symbol *name, t_atom_long num_reserved_entries, t_atom_long num_columns);
void entry_database_free(t_entry_database *x);

int entry_database_init()
{
    database_class = class_new ("__entry_database",
                             (method)entry_database_new,
                             (method)entry_database_free,
                             (short)sizeof(t_entry_database),
                             NULL,
                             A_SYM,
                             A_LONG,
                             A_LONG,
                             0);
    
    class_register(CLASS_NOBOX, database_class);
    
    return 0;
}

void *entry_database_new(t_symbol *name, t_atom_long num_reserved_entries, t_atom_long num_columns)
{
    t_entry_database *x = (t_entry_database *)object_alloc(database_class);
    
    num_reserved_entries = std::max(num_reserved_entries, t_atom_long(1));
    num_columns = std::max(num_columns, t_atom_long(1));
    
    x->database = new EntryDatabase(name, num_columns);
    x->database->reserve(num_reserved_entries);
    x->count = 1;
    
    x->notify = true;
    
    return (x);
}

void entry_database_free(t_entry_database *x)
{
    delete x->database;
}

void entry_database_release(t_entry_database *x)
{
    if (x && --x->count == 0)
    {
        object_unregister(x);
        object_free(x);
    }
}

t_entry_database *entry_database_find(t_symbol *name, t_entry_database *old_object)
{
    // Make sure the max database class exists
    
    if (!class_findbyname(CLASS_NOBOX, gensym(database_class_name)))
        entry_database_init();
    
    // See if an object is registered with this name
    
    t_entry_database *x = (t_entry_database *) object_findregistered(name_space_name, name);
    
    // If the object registered is different
    
    if (x && x != old_object)
    {
        if (old_object)
            entry_database_release(old_object);
        x->count++;
    }
    else
        x = old_object;
    
    return x;
}

t_entry_database *entry_database_create(t_symbol *name, t_atom_long num_reserved_entries, t_atom_long num_columns)
{
    t_atom argv[3];
    atom_setsym(argv + 0, name);
    atom_setlong(argv + 1, num_reserved_entries);
    atom_setlong(argv + 2, num_columns);
    
    // See if an object is registered (otherwise make object and register it...)
    
    t_entry_database *x = entry_database_find(name, NULL);
    
    if (!x)
        x = (t_entry_database *) object_register(name_space_name, name, object_new_typed(CLASS_NOBOX, gensym(database_class_name), 3, argv));
    
    return x;
}

t_object *database_create(t_symbol *name, t_atom_long num_reserved_entries, t_atom_long num_columns)
{
    return (t_object *) entry_database_create(name, num_reserved_entries, num_columns);
}

t_object *database_change(t_symbol *name, t_object *old_object)
{
    return (t_object *) entry_database_find(name, (t_entry_database *) old_object);
}

void database_release(t_object *x)
{
    entry_database_release((t_entry_database *) x);
}

EntryDatabase::ReadPointer database_getptr_read(t_object *x)
{
    t_entry_database *obj = (t_entry_database *) x;
    return EntryDatabase::ReadPointer(obj->database);
}

NotifyPointer database_getptr_write(t_object *x, t_object *client)
{
    t_entry_database *obj = (t_entry_database *) x;
    return NotifyPointer(obj->database, (t_object *)obj, client);

}

    