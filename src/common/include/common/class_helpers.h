

#define MAKE_GETTER_SETTER_WC(type, name, varName, onChange)                   \
    const type &get##name() const {                                            \
        return varName;                                                        \
    }                                                                          \
    void set##name(const type &value) {                                        \
        varName = value;                                                       \
        onChange();                                                            \
    }                                                                          \
    type &get##name() {                                                        \
        return varName;                                                        \
    }

#define MAKE_GETTER_SETTER(type, name, varName)                                \
    const type &get##name() const {                                            \
        return varName;                                                        \
    }                                                                          \
    void set##name(const type &value) {                                        \
        varName = value;                                                       \
    }                                                                          \
    type &get##name() {                                                        \
        return varName;                                                        \
    }

#define MAKE_GETTER(type, name, varName)                                       \
    const type &get##name() const {                                            \
        return varName;                                                        \
    }                                                                          \
    type &get##name() {                                                        \
        return varName;                                                        \
    }

#define MAKE_VGETTER_VSETTER(type, name, varName)                              \
    virtual const type &get##name() const {                                    \
        return varName;                                                        \
    }                                                                          \
    virtual void set##name(const type &value) {                                \
        varName = value;                                                       \
    }                                                                          \
    virtual type &get##name() {                                                \
        return varName;                                                        \
    }

#define MAKE_GETTER(type, name, varName)                                       \
    const type &get##name() const {                                            \
        return varName;                                                        \
    }                                                                          \
    type &get##name() {                                                        \
        return varName;                                                        \
    }

#define MAKE_SETTER(type, name, varName)                                       \
    void set##name(const type &value) {                                        \
        varName = value;                                                       \
    }

#define MAKE_GETTER_SETTER_NR(type, name, varName)                             \
    const type &get##name() const {                                            \
        return varName;                                                        \
    }                                                                          \
    void set##name(const type &value) {                                        \
        varName = value;                                                       \
    }

// before change callback, and after change callback
#define MAKE_GETTER_SETTER_BC_AC(                                              \
    type, name, varName, onBeforeChange, onAfterChange)                        \
    const type &get##name() const {                                            \
        return varName;                                                        \
    }                                                                          \
    void set##name(const type &value) {                                        \
        onBeforeChange(value);                                                 \
        varName = value;                                                       \
        onAfterChange();                                                       \
    }                                                                          \
    type &get##name() {                                                        \
        return varName;                                                        \
    }

#define MAKE_GETTER_SETTER_MT(type, name, varName, mutex)                      \
    const type &get##name() const {                                            \
        return varName;                                                        \
    }                                                                          \
    void set##name(const type &value) {                                        \
        std::lock_guard lk(mutex);                                             \
        varName = value;                                                       \
    }                                                                          \
    type &get##name() {                                                        \
        return varName;                                                        \
    }

#define DEFAULT_CONTRS(className)                                              \
    className() = default;                                                     \
    className(const className &) = default;                                    \
    className(className &&) = default;                                         \
    ~className() = default;                                                    \
    className &operator=(const className &) = default;                         \
    className &operator=(className &&) = default;

#define DEFAULT_CONTRS_VDES(className)                                         \
    className() = default;                                                     \
    className(const className &) = default;                                    \
    className(className &&) = default;                                         \
    virtual ~className() = default;                                            \
    className &operator=(const className &) = default;                         \
    className &operator=(className &&) = default;

#define DEFAULT_CONTRS_NODES(className)                                        \
    className() = default;                                                     \
    className(const className &) = default;                                    \
    className(className &&) = default;                                         \
    className &operator=(const className &) = default;                         \
    className &operator=(className &&) = default;

#define MAKE_GETTER_SETTER_PTR(type, name, varName)                            \
    const type *get##name() const {                                            \
        return varName;                                                        \
    }                                                                          \
    void set##name(type *value) {                                              \
        varName = value;                                                       \
    }                                                                          \
    type *get##name() {                                                        \
        return varName;                                                        \
    }
