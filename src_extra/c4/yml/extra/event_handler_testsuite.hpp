#ifndef _C4_YML_EXTRA_EVENT_HANDLER_TESTSUITE_HPP_
#define _C4_YML_EXTRA_EVENT_HANDLER_TESTSUITE_HPP_

#ifndef RYML_SINGLE_HEADER
#ifndef _C4_YML_EVENT_HANDLER_STACK_HPP_
#include "c4/yml/event_handler_stack.hpp"
#endif
#ifndef _C4_YML_TAG_HPP_
#include "c4/yml/tag.hpp"
#endif
#endif
#ifndef _C4_YML_EXTRA_STRING_HPP_
#include "c4/yml/extra/string.hpp"
#endif

C4_SUPPRESS_WARNING_GCC_CLANG_PUSH
C4_SUPPRESS_WARNING_GCC_CLANG("-Wold-style-cast")
C4_SUPPRESS_WARNING_GCC("-Wuseless-cast")


namespace c4 {
namespace yml {
namespace extra {


/** @addtogroup doc_event_handlers
 * @{ */


/** @cond dev */
struct EventHandlerTestSuiteState : public ParserState
{
    NodeData ev_data;
};
void append_scalar_escaped(extra::string *s, csubstr val);
/** @endcond */

/** This event produces standard YAML events as used in the
 * [YAML test suite](https://github.com/yaml/yaml-test-suite).
 * See the documentation for @ref doc_event_handlers, which has
 * important notes about the event model used by rapidyaml.
 *
 * This class is used only in the CI of this project, and in the
 * application used as part of the [standard YAML
 * playground](https://play.yaml.io/main/parser). It is not part of
 * the library and is not installed. *
 */
struct EventHandlerTestSuite : public EventHandlerStack<EventHandlerTestSuite, EventHandlerTestSuiteState>
{

    /** @name types
     * @{ */

    // our internal state must inherit from parser state
    using state = EventHandlerTestSuiteState;

    using EventSink = extra::string;

    /** @} */

public:

    /** @cond dev */
    EventSink *C4_RESTRICT m_sink;
    extra::string_vector m_val_buffers;
    id_type m_curr_doc;
    extra::string m_arena;
    TagDirectives m_tag_directives;

    // undefined at the end
    #define _enable_(bits) _enable__<bits>()
    #define _disable_(bits) _disable__<bits>()
    #define _has_any_(bits) _has_any__<bits>()

    void _dbg_print() const
    {
        #ifdef RYML_DBG
        _dbg_printf("\n");
        auto indent = [](id_type n){
            for(id_type level = 0; level < n; ++level)
            {
                _dbg_printf("  ");
            }
        };
        for(id_type i = 0; i < m_stack.size(); ++i)
        {
            csubstr const& str = _buf_(i);
            indent(i);
            _dbg_printf("[{}] @id={}\n", i, m_stack[i].node_id);
            for(csubstr ln : str.split('\n'))
            {
                indent(i);
                _dbg_printf("{}\n", ln);
            }
        }
        #endif
    }
    /** @endcond */

public:

    /** @name construction and resetting
     * @{ */

    EventHandlerTestSuite(EventSink *sink, Callbacks const& cb) : EventHandlerStack(cb), m_sink(sink), m_val_buffers(), m_curr_doc(), m_arena(), m_tag_directives()
    {
        reset();
    }
    EventHandlerTestSuite(Callbacks const& cb) : EventHandlerTestSuite(nullptr, cb) {}
    EventHandlerTestSuite(EventSink *sink) : EventHandlerTestSuite(sink, get_callbacks()) {}
    EventHandlerTestSuite() : EventHandlerTestSuite(nullptr, get_callbacks()) {}

    void reset()
    {
        _stack_reset_root();
        m_curr->flags |= RUNK|RTOP;
        m_val_buffers.clear();
        m_val_buffers.resize((size_t)m_stack.size());
        m_arena.clear();
        m_arena.reserve(1024);
        m_src = {};
        m_curr_doc = 0;
        m_tag_directives.clear();
    }

    C4_ALWAYS_INLINE TagDirectives &tag_directives() { return m_tag_directives; }

    /** @} */

public:

    /** @name parse events
     * @{ */

    void start_parse(const char* filename, substr src)
    {
        this->_stack_start_parse(filename, src);
    }

    void finish_parse()
    {
        this->_stack_finish_parse();
    }

    void cancel_parse()
    {
        while(m_stack.size() > 1)
            _pop();
        _buf_flush_();
    }

    /** @} */

public:

    /** @name YAML stream events */
    /** @{ */

    void begin_stream()
    {
        _send_("+STR\n");
    }

    void end_stream()
    {
        _send_("-STR\n");
        _buf_flush_();
    }

    /** @} */

public:

    /** @name YAML document events */
    /** @{ */

    /** implicit doc start (without ---) */
    void begin_doc()
    {
        _c4dbgp("begin_doc");
        if(_stack_should_push_on_begin_doc())
        {
            _c4dbgp("push!");
            ++m_curr->node_id;
            _push();
            _enable_(DOC);
        }
        _send_("+DOC\n");
    }
    /** implicit doc end (without ...) */
    void end_doc()
    {
        _c4dbgp("end_doc");
        _send_("-DOC\n");
        if(_stack_should_pop_on_end_doc())
        {
            _c4dbgp("pop!");
            _pop();
        }
        ++m_curr_doc;
    }

    /** explicit doc start, with --- */
    void begin_doc_expl()
    {
        _c4dbgp("begin_doc_expl");
        if(_stack_should_push_on_begin_doc())
        {
            _c4dbgp("push!");
            ++m_curr->node_id;
            _push();
        }
        _send_("+DOC ---\n");
        _enable_(DOC);
    }
    /** explicit doc end, with ... */
    void end_doc_expl()
    {
        _c4dbgp("end_doc_expl");
        _send_("-DOC ...\n");
        if(_stack_should_pop_on_end_doc())
        {
            _c4dbgp("pop!");
            _pop();
        }
        ++m_curr_doc;
    }

    /** @} */

public:

    /** @name YAML map functions */
    /** @{ */

    void begin_map_key_flow()
    {
        _RYML_CHECK_BASIC_(m_stack.m_callbacks, !_has_any_(VAL));
        _send_("+MAP {}");
        _send_key_props_();
        _send_('\n');
        _mark_parent_with_children_();
        _enable_(MAP|FLOW_SL);
        _push();
        ++m_curr->node_id;
    }
    void begin_map_key_block()
    {
        _RYML_CHECK_BASIC_(m_stack.m_callbacks, !_has_any_(VAL));
        _send_("+MAP");
        _send_key_props_();
        _send_('\n');
        _mark_parent_with_children_();
        _enable_(MAP|BLOCK);
        _push();
        ++m_curr->node_id;
    }

    void begin_map_val_flow()
    {
        _RYML_CHECK_BASIC_(m_stack.m_callbacks, !_has_any_(VAL));
        _send_("+MAP {}");
        _send_val_props_();
        _send_('\n');
        _mark_parent_with_children_();
        _enable_(MAP|FLOW_SL);
        _push();
        ++m_curr->node_id;
    }
    void begin_map_val_block()
    {
        _RYML_CHECK_BASIC_(m_stack.m_callbacks, !_has_any_(VAL));
        _send_("+MAP");
        _send_val_props_();
        _send_('\n');
        _mark_parent_with_children_();
        _enable_(MAP|BLOCK);
        _push();
        ++m_curr->node_id;
    }

    void end_map_block()
    {
        _pop();
        _send_("-MAP\n");
    }

    void end_map_flow(bool /*multiline*/)
    {
        _pop();
        _send_("-MAP\n");
    }

    /** @} */

public:

    /** @name YAML seq events */
    /** @{ */

    void begin_seq_key_flow()
    {
        _RYML_CHECK_BASIC_(m_stack.m_callbacks, !_has_any_(VAL));
        _send_("+SEQ []");
        _send_key_props_();
        _send_('\n');
        _mark_parent_with_children_();
        _enable_(SEQ|FLOW_SL);
        _push();
        ++m_curr->node_id;
    }
    void begin_seq_key_block()
    {
        _RYML_CHECK_BASIC_(m_stack.m_callbacks, !_has_any_(VAL));
        _send_("+SEQ");
        _send_key_props_();
        _send_('\n');
        _mark_parent_with_children_();
        _enable_(SEQ|BLOCK);
        _push();
        ++m_curr->node_id;
    }

    void begin_seq_val_flow()
    {
        _RYML_CHECK_BASIC_(m_stack.m_callbacks, !_has_any_(VAL));
        _send_("+SEQ []");
        _send_val_props_();
        _send_('\n');
        _mark_parent_with_children_();
        _enable_(SEQ|FLOW_SL);
        _push();
        ++m_curr->node_id;
    }
    void begin_seq_val_block()
    {
        _RYML_CHECK_BASIC_(m_stack.m_callbacks, !_has_any_(VAL));
        _send_("+SEQ");
        _send_val_props_();
        _send_('\n');
        _mark_parent_with_children_();
        _enable_(SEQ|BLOCK);
        _push();
        ++m_curr->node_id;
    }

    void end_seq_block()
    {
        _pop();
        _send_("-SEQ\n");
    }

    void end_seq_flow(bool /*multiline*/)
    {
        _pop();
        _send_("-SEQ\n");
    }

    /** @} */

public:

    /** @name YAML structure events */
    /** @{ */

    void add_sibling()
    {
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, m_parent);
        _buf_flush_to_(m_curr->level, m_parent->level);
        m_curr->ev_data = {};
        ++m_curr->node_id;
    }

    /** set the previous val as the first key of a new map, with flow style.
     *
     * See the documentation for @ref doc_event_handlers, which has
     * important notes about this event.
     */
    void actually_val_is_first_key_of_new_map_flow()
    {
        _c4dbgpf("node[{}]: actually_val_is_first_key_of_new_map_flow", m_curr->node_id);
        // ensure we have a temporary buffer to save the current val
        const id_type tmp = m_curr->level + id_type(2);
        _buf_ensure_(tmp + id_type(2));
        // save the current val to the temporary buffer
        _buf_flush_to_(m_curr->level, tmp);
        _disable_(_VALMASK|VAL_STYLE);
        // create the map.
        // this will push a new level, and tmp is one further
        begin_map_val_flow();
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, tmp != m_curr->level);
        // now move the saved val as the first key
        _buf_flush_to_(tmp, m_curr->level);
        ++m_curr->node_id;
    }

    /** like its flow counterpart, but this function can only be
     * called after the end of a flow-val at root or doc level.
     *
     * See the documentation for @ref doc_event_handlers, which has
     * important notes about this event.
     */
    void actually_val_is_first_key_of_new_map_block()
    {
        _c4dbgpf("node[{}]: actually_val_is_first_key_of_new_map_block", m_curr->node_id);
        EventSink &sink = _buf_();
        csubstr full = sink;(void)full;
        // interpolate +MAP\n after the last +DOC\n
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, full.len);
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, !full.count('\r'));
        size_t docpos = sink.find_last("+DOC\n");
        if(docpos != npos)
        {
            _RYML_ASSERT_BASIC_(m_stack.m_callbacks, (m_stack.size() == 1u) ? (docpos >= 5u) : (docpos == 0u));
            _RYML_ASSERT_BASIC_(m_stack.m_callbacks, docpos + 5u < full.len);
            sink.insert("+MAP\n", docpos + 5u);
        }
        else
        {
            // ... or interpolate +MAP\n after the last +DOC ---\n
            docpos = sink.find_last("+DOC ---\n");
            if(docpos != npos)
            {
                _RYML_ASSERT_BASIC_(m_stack.m_callbacks, (m_stack.size() == 1u) ? (docpos >= 5u) : (docpos == 0u));
                _RYML_ASSERT_BASIC_(m_stack.m_callbacks, docpos + 9u < full.len);
                sink.insert("+MAP\n", docpos + 9u);
            }
            else
            {
                sink.insert("+MAP\n", 0u);
            }
        }
        _push();
        ++m_curr->node_id;
    }

    /** @} */

public:

    /** @name YAML scalar events */
    /** @{ */


    C4_ALWAYS_INLINE void set_key_scalar_plain_empty() noexcept
    {
        _c4dbgpf("node[{}]: set key scalar plain as empty", m_curr->node_id);
        _send_key_scalar_({}, ':');
        _enable_(KEY|KEY_PLAIN|KEYNIL);
    }
    C4_ALWAYS_INLINE void set_val_scalar_plain_empty() noexcept
    {
        _c4dbgpf("node[{}]: set val scalar plain as empty", m_curr->node_id);
        _send_val_scalar_({}, ':');
        _enable_(VAL|VAL_PLAIN|VALNIL);
    }

    C4_ALWAYS_INLINE void set_key_scalar_plain(csubstr scalar)
    {
        _c4dbgpf("node[{}]: set key scalar plain: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        _send_key_scalar_(scalar, ':');
        _enable_(KEY|KEY_PLAIN);
    }
    C4_ALWAYS_INLINE void set_val_scalar_plain(csubstr scalar)
    {
        _c4dbgpf("node[{}]: set val scalar plain: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        _send_val_scalar_(scalar, ':');
        _enable_(VAL|VAL_PLAIN);
    }


    C4_ALWAYS_INLINE void set_key_scalar_dquoted(csubstr scalar)
    {
        _c4dbgpf("node[{}]: set key scalar dquot: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        _send_key_scalar_(scalar, '"');
        _enable_(KEY|KEY_DQUO);
    }
    C4_ALWAYS_INLINE void set_val_scalar_dquoted(csubstr scalar)
    {
        _c4dbgpf("node[{}]: set val scalar dquot: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        _send_val_scalar_(scalar, '"');
        _enable_(VAL|VAL_DQUO);
    }


    C4_ALWAYS_INLINE void set_key_scalar_squoted(csubstr scalar)
    {
        _c4dbgpf("node[{}]: set key scalar squot: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        _send_key_scalar_(scalar, '\'');
        _enable_(KEY|KEY_SQUO);
    }
    C4_ALWAYS_INLINE void set_val_scalar_squoted(csubstr scalar)
    {
        _c4dbgpf("node[{}]: set val scalar squot: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        _send_val_scalar_(scalar, '\'');
        _enable_(VAL|VAL_SQUO);
    }


    C4_ALWAYS_INLINE void set_key_scalar_literal(csubstr scalar)
    {
        _c4dbgpf("node[{}]: set key scalar literal: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        _send_key_scalar_(scalar, '|');
        _enable_(KEY|KEY_LITERAL);
    }
    C4_ALWAYS_INLINE void set_val_scalar_literal(csubstr scalar)
    {
        _c4dbgpf("node[{}]: set val scalar literal: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        _send_val_scalar_(scalar, '|');
        _enable_(VAL|VAL_LITERAL);
    }


    C4_ALWAYS_INLINE void set_key_scalar_folded(csubstr scalar)
    {
        _c4dbgpf("node[{}]: set key scalar folded: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        _send_key_scalar_(scalar, '>');
        _enable_(KEY|KEY_FOLDED);
    }
    C4_ALWAYS_INLINE void set_val_scalar_folded(csubstr scalar)
    {
        _c4dbgpf("node[{}]: set val scalar folded: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        _send_val_scalar_(scalar, '>');
        _enable_(VAL|VAL_FOLDED);
    }


    C4_ALWAYS_INLINE void mark_key_scalar_unfiltered()
    {
        // nothing to do here
    }
    C4_ALWAYS_INLINE void mark_val_scalar_unfiltered()
    {
        // nothing to do here
    }

    /** @} */

public:

    /** @name YAML anchor/reference events */
    /** @{ */

    void set_key_anchor(csubstr anchor)
    {
        _c4dbgpf("node[{}]: set key anchor: [{}]~~~{}~~~", m_curr->node_id, anchor.len, anchor);
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, !_has_any_(KEYREF));
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, !anchor.begins_with('&'));
        _enable_(KEYANCH);
        m_curr->ev_data.m_key.anchor = anchor;
    }
    void set_val_anchor(csubstr anchor)
    {
        _c4dbgpf("node[{}]: set val anchor: [{}]~~~{}~~~", m_curr->node_id, anchor.len, anchor);
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, !_has_any_(VALREF));
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, !anchor.begins_with('&'));
        _enable_(VALANCH);
        m_curr->ev_data.m_val.anchor = anchor;
    }

    void set_key_ref(csubstr ref)
    {
        _c4dbgpf("node[{}]: set key ref: [{}]~~~{}~~~", m_curr->node_id, ref.len, ref);
        if(C4_UNLIKELY(_has_any_(KEYANCH)))
            _RYML_ERR_PARSE_(m_stack.m_callbacks, m_curr->pos, "key cannot have both anchor and ref");
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, ref.begins_with('*'));
        _enable_(KEY|KEYREF);
        _send_("=ALI ");
        _send_(ref);
        _send_('\n');
    }
    void set_val_ref(csubstr ref)
    {
        _c4dbgpf("node[{}]: set val ref: [{}]~~~{}~~~", m_curr->node_id, ref.len, ref);
        if(C4_UNLIKELY(_has_any_(VALANCH)))
            _RYML_ERR_PARSE_(m_stack.m_callbacks, m_curr->pos, "val cannot have both anchor and ref");
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, ref.begins_with('*'));
        _enable_(VAL|VALREF);
        _send_("=ALI ");
        _send_(ref);
        _send_('\n');
    }

    /** @} */

public:

    /** @name YAML tag events */
    /** @{ */

    void set_key_tag(csubstr tag)
    {
        _c4dbgpf("node[{}]: set key tag: [{}]~~~{}~~~", m_curr->node_id, tag.len, tag.str ? tag : csubstr("(no space)"));
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, tag.str);
        _enable_(KEYTAG);
        m_curr->ev_data.m_key.tag = tag;
    }
    void set_val_tag(csubstr tag)
    {
        _c4dbgpf("node[{}]: set val tag: [{}]~~~{}~~~", m_curr->node_id, tag.len, tag.str ? tag : csubstr("(no space)"));
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, tag.str);
        _enable_(VALTAG);
        m_curr->ev_data.m_val.tag = tag;
    }

    /** @} */

public:

    /** @name YAML directive events */
    /** @{ */

    void add_directive_yaml(csubstr yaml_version)
    {
        (void)yaml_version;
        _c4dbgpf("%YAML directive! {}", yaml_version);
    }

    void add_directive_tag(csubstr handle, csubstr prefix)
    {
        _c4dbgpf("%TAG directive! handle={} prefix={} doc_id={}", handle, prefix, m_curr_doc);
        if(C4_UNLIKELY(!m_tag_directives.add(handle, prefix, m_curr_doc)))
            _RYML_ERR_PARSE_(m_stack.m_callbacks, m_curr->pos, "too many %TAG directives");
    }

    /** @} */

public:

    /** @name YAML arena events */
    /** @{ */

    substr arena()
    {
        return to_substr(m_arena);
    }
    substr arena_rem()
    {
        return substr(m_arena).last(0);
    }
    substr alloc_arena(size_t len)
    {
        const size_t sz = m_arena.size();
        m_arena.resize(sz + len);
        return to_substr(m_arena).sub(sz);
    }

    /** @} */

public:

    /** @cond dev */

    /** push a new parent */
    void _push()
    {
        id_type curr = m_curr->node_id;
        _stack_push();
        _buf_ensure_(m_stack.size() + id_type(1));
        _buf_().clear();
        m_curr->ev_data = {};
        m_curr->node_id = curr;
        _c4dbgpf("pushed! level={}", m_curr->level);
    }

    /** end the current scope */
    void _pop()
    {
        id_type curr = m_curr->node_id;
        _buf_flush_to_(m_curr->level, m_parent->level);
        _stack_pop();
        m_curr->node_id = curr;
    }

    template<type_bits bits> C4_ALWAYS_INLINE void _enable__() noexcept
    {
        m_curr->ev_data.m_type.type = static_cast<NodeType_e>(m_curr->ev_data.m_type.type | bits);
    }
    template<type_bits bits> C4_ALWAYS_INLINE void _disable__() noexcept
    {
        m_curr->ev_data.m_type.type = static_cast<NodeType_e>(m_curr->ev_data.m_type.type & (~bits));
    }
    template<type_bits bits> C4_ALWAYS_INLINE bool _has_any__() const noexcept
    {
        return (m_curr->ev_data.m_type.type & bits) != 0;
    }

    void _mark_parent_with_children_()
    {
        if(m_parent)
            m_parent->has_children = true;
    }

    EventSink& _buf_() noexcept
    {
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, m_curr->level < m_val_buffers.size());
        return m_val_buffers[m_curr->level];
    }

    EventSink& _buf_(id_type level) noexcept
    {
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, level < m_val_buffers.size());
        return m_val_buffers[level];
    }

    EventSink const& _buf_(id_type level) const noexcept
    {
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, level < m_val_buffers.size());
        return m_val_buffers[level];
    }

    static void _buf_flush_to_(EventSink &C4_RESTRICT src, EventSink &C4_RESTRICT dst) noexcept
    {
        dst.append(src);
        src.clear();
    }

    void _buf_flush_to_(id_type level_src, id_type level_dst) noexcept
    {
        auto &src = _buf_(level_src);
        auto &dst = _buf_(level_dst);
        _buf_flush_to_(src, dst);
    }

    void _buf_flush_() noexcept
    {
        _buf_flush_to_(_buf_(), *m_sink);
    }

    void _buf_ensure_(id_type size_needed) noexcept
    {
        if(size_needed > m_val_buffers.size())
            m_val_buffers.resize(size_needed);
    }

    C4_ALWAYS_INLINE void _send_(csubstr s) noexcept { _buf_().append(s); }
    C4_ALWAYS_INLINE void _send_(char c) noexcept { _buf_().append(c); }

    void _send_key_scalar_(csubstr scalar, char scalar_type_code)
    {
        _send_("=VAL");
        _send_key_props_();
        _send_(' ');
        _send_(scalar_type_code);
        append_scalar_escaped(&_buf_(), scalar);
        _send_('\n');
    }
    void _send_val_scalar_(csubstr scalar, char scalar_type_code)
    {
        _send_("=VAL");
        _send_val_props_();
        _send_(' ');
        _send_(scalar_type_code);
        append_scalar_escaped(&_buf_(), scalar);
        _send_('\n');
    }

    void _send_key_props_()
    {
        if(_has_any_(KEYANCH|KEYREF))
        {
            _send_(" &");
            _send_(m_curr->ev_data.m_key.anchor);
        }
        if(_has_any_(KEYTAG))
        {
            _send_tag_(m_curr->ev_data.m_key.tag);
        }
        m_curr->ev_data.m_key = {};
        _disable_(KEYANCH|KEYREF|KEYTAG);
    }
    void _send_val_props_()
    {
        if(_has_any_(VALANCH|VALREF))
        {
            _send_(" &");
            _send_(m_curr->ev_data.m_val.anchor);
        }
        if(m_curr->ev_data.m_type.type & VALTAG)
        {
            _send_tag_(m_curr->ev_data.m_val.tag);
        }
        m_curr->ev_data.m_val = {};
        _disable_(VALANCH|VALREF|VALTAG);
    }
    void _send_tag_(csubstr tag)
    {
        _RYML_ASSERT_BASIC_(m_stack.m_callbacks, !tag.empty());
        if(tag.str[0] == '<')
        {
            _send_(' ');
            _send_(tag);
        }
        else if(tag.begins_with("!<"))
        {
            _send_(' ');
            _send_(tag.sub(1));
        }
        else
        {
            _send_(" <");
            _send_(tag);
            _send_('>');
        }
    }

    #undef _enable_
    #undef _disable_
    #undef _has_any_

    /** @endcond */
};

/** @} */

} // namespace extra
} // namespace yml
} // namespace c4

C4_SUPPRESS_WARNING_GCC_POP

#endif /* _C4_YML_EVT_EXTRA_EVENT_HANDLER_TESTSUITE_HPP_ */
