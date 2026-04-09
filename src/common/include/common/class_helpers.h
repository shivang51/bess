

#define MAKE_GETTER_SETTER_WC(type, name, varName, onChange) \
    const type &get##name() const { return varName; }        \
    void set##name(const type &value) {                      \
        varName = value;                                     \
        onChange();                                          \
    }                                                        \
    type &get##name() { return varName; }

#define MAKE_GETTER_SETTER(type, name, varName)       \
    const type &get##name() const { return varName; } \
    void set##name(const type &value) {               \
        varName = value;                              \
    }                                                 \
    type &get##name() { return varName; }

#define MAKE_GETTER(type, name, varName)              \
    const type &get##name() const { return varName; } \
    type &get##name() { return varName; }

#define MAKE_VGETTER_VSETTER(type, name, varName)             \
    virtual const type &get##name() const { return varName; } \
    virtual void set##name(const type &value) {               \
        varName = value;                                      \
    }                                                         \
    virtual type &get##name() { return varName; }

#define MAKE_GETTER(type, name, varName)              \
    const type &get##name() const { return varName; } \
    type &get##name() { return varName; }

#define MAKE_SETTER(type, name, varName) \
    void set##name(const type &value) {  \
        varName = value;                 \
    }

#define MAKE_GETTER_SETTER_NR(type, name, varName)    \
    const type &get##name() const { return varName; } \
    void set##name(const type &value) {               \
        varName = value;                              \
    }

// before change callback, and after change callback
#define MAKE_GETTER_SETTER_BC_AC(type, name, varName, onBeforeChange, onAfterChange) \
    const type &get##name() const { return varName; }                                \
    void set##name(const type &value) {                                              \
        onBeforeChange();                                                            \
        varName = value;                                                             \
        onAfterChange();                                                             \
    }                                                                                \
    type &get##name() { return varName; }
