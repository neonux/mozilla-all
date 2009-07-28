# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# Contributor(s):
#   Chris Jones <jones.chris.g@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

import sys

class Visitor:
    def defaultVisit(self, node):
        raise Exception, "INTERNAL ERROR: no visitor for node type `%s'"% (
            node.__class__.__name__)

    def visitTranslationUnit(self, tu):
        for cxxInc in tu.cxxIncludes:
            cxxInc.accept(self)
        for protoInc in tu.protocolIncludes:
            protoInc.accept(self)
        for using in tu.using:
            using.accept(self)
        tu.protocol.accept(self)

    def visitCxxInclude(self, inc):
        pass

    def visitProtocolInclude(self, inc):
        # Note: we don't visit the child AST here, because that needs delicate
        # and pass-specific handling
        pass

    def visitUsingStmt(self, using):
        pass

    def visitProtocol(self, p):
        for namespace in p.namespaces:
            namespace.accept(self)
        if p.manager is not None:
            p.manager.accept(self)
        for managed in p.managesStmts:
            managed.accept(self)
        for msgDecl in p.messageDecls:
            msgDecl.accept(self)
        for transitionStmt in p.transitionStmts:
            transitionStmt.accept(self)

    def visitNamespace(self, ns):
        pass

    def visitManagerStmt(self, mgr):
        pass

    def visitManagesStmt(self, mgs):
        pass

    def visitMessageDecl(self, md):
        for inParam in md.inParams:
            inParam.accept(self)
        for outParam in md.outParams:
            outParam.accept(self)

    def visitTransitionStmt(self, ts):
        ts.state.accept(self)
        for trans in ts.transitions:
            trans.accept(self)

    def visitTransition(self, t):
        t.toState.accept(self)

    def visitState(self, s):
        pass

    def visitParam(self, decl):
        pass

    def visitTypeSpec(self, ts):
        pass

    def visitDecl(self, d):
        pass

class Loc:
    def __init__(self, filename='<??>', lineno=0):
        assert filename
        self.filename = filename
        self.lineno = lineno
    def __repr__(self):
        return '%r:%r'% (self.filename, self.lineno)
    def __str__(self):
        return '%s:%s'% (self.filename, self.lineno)

Loc.NONE = Loc(filename='<??>', lineno=0)

class _struct():
    pass

class Node:
    def __init__(self, loc=Loc.NONE):
        self.loc = loc

    def accept(self, visitor):
        visit = getattr(visitor, 'visit'+ self.__class__.__name__, None)
        if visit is None:
            return getattr(visitor, 'defaultVisit')(self)
        return visit(self)

    def addAttrs(self, attrsName):
        if not hasattr(self, attrsName):
            setattr(self, attrsName, _struct())

class TranslationUnit(Node):
    def __init__(self):
        Node.__init__(self)
        self.filename = None
        self.cxxIncludes = [ ]
        self.protocolIncludes = [ ]
        self.using = [ ]
        self.protocol = None

    def addCxxInclude(self, cxxInclude): self.cxxIncludes.append(cxxInclude)
    def addProtocolInclude(self, pInc): self.protocolIncludes.append(pInc)
    def addUsingStmt(self, using): self.using.append(using)

    def setProtocol(self, protocol): self.protocol = protocol

class CxxInclude(Node):
    def __init__(self, loc, cxxFile):
        Node.__init__(self, loc)
        self.file = cxxFile

class ProtocolInclude(Node):
    def __init__(self, loc, protocolFile):
        Node.__init__(self, loc)
        self.file = protocolFile

class UsingStmt(Node):
    def __init__(self, loc, cxxTypeSpec):
        Node.__init__(self, loc)
        self.type = cxxTypeSpec

# "singletons"
class ASYNC:
    pretty = 'async'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def __str__(cls):  return cls.pretty
class RPC:
    pretty = 'rpc'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def __str__(cls):  return cls.pretty
class SYNC:
    pretty = 'sync'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def __str__(cls):  return cls.pretty

class INOUT:
    pretty = 'inout'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def __str__(cls):  return cls.pretty
class IN:
    pretty = 'in'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def __str__(cls):  return cls.pretty
    @staticmethod
    def prettySS(cls, ss): return _prettyTable['in'][ss.pretty]
class OUT:
    pretty = 'out'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def __str__(cls):  return cls.pretty
    @staticmethod
    def prettySS(ss): return _prettyTable['out'][ss.pretty]

_prettyTable = {
    IN  : { 'async': 'AsyncRecv',
            'sync': 'SyncRecv',
            'rpc': 'RpcAnswer' },
    OUT : { 'async': 'AsyncSend',
            'sync': 'SyncSend',
            'rpc': 'RpcCall' }
    # inout doesn't make sense here
}


class Protocol(Node):
    def __init__(self, loc):
        Node.__init__(self, loc)
        self.name = None
        self.namespaces = [ ]
        self.sendSemantics = ASYNC
        self.managesStmts = [ ]
        self.messageDecls = [ ]
        self.transitionStmts = [ ]

    def addOuterNamespace(self, namespace):
        self.namespaces.insert(0, namespace)

    def addManagesStmts(self, managesStmts):
        self.managesStmts += managesStmts

    def addMessageDecls(self, messageDecls):
        self.messageDecls += messageDecls

    def addTransitionStmts(self, transStmts):
        self.transitionStmts += transStmts

class Namespace(Node):
    def __init__(self, loc, namespace):
        Node.__init__(self, loc)
        self.namespace = namespace

class ManagerStmt(Node):
    def __init__(self, loc, managerName):
        Node.__init__(self, loc)
        self.name = managerName

class ManagesStmt(Node):
    def __init__(self, loc, managedName):
        Node.__init__(self, loc)
        self.name = managedName

class MessageDecl(Node):
    def __init__(self, loc):
        Node.__init__(self, loc)
        self.name = None
        self.sendSemantics = ASYNC
        self.direction = None
        self.inParams = [ ]
        self.outParams = [ ]

    def addInParams(self, inParamsList):
        self.inParams += inParamsList

    def addOutParams(self, outParamsList):
        self.outParams += outParamsList

    def hasReply(self):
        return self.sendSemantics is SYNC or self.sendSemantics is RPC

class TransitionStmt(Node):
    def __init__(self, loc, state, transitions):
        Node.__init__(self, loc)
        self.state = state
        self.transitions = transitions

class Transition(Node):
    def __init__(self, loc, trigger, msg, toState):
        Node.__init__(self, loc)
        self.trigger = trigger
        self.msg = msg
        self.toState = toState

    @staticmethod
    def nameToTrigger(name):
        return { 'send': SEND, 'recv': RECV, 'call': CALL, 'answer': ANSWER }[name]

class SEND:
    pretty = 'send'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def direction(cls): return OUT
class RECV:
    pretty = 'recv'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def direction(cls): return IN
class CALL:
    pretty = 'call'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def direction(cls): return OUT
class ANSWER:
    pretty = 'answer'
    @classmethod
    def __hash__(cls): return hash(cls.pretty)
    @classmethod
    def direction(cls): return IN

class State(Node):
    def __init__(self, loc, name):
        Node.__init__(self, loc)
        self.name = name
    def __eq__(self, o):
        return isinstance(o, State) and o.name == self.name
    def __hash__(self):
        return hash(repr(self))
    def __ne__(self, o):
        return not (self == o)
    def __repr__(self): return '<State %r>'% (self.name)
    def __str__(self): return '<State %s>'% (self.name)

class Param(Node):
    def __init__(self, loc, typespec, name):
        Node.__init__(self, loc)
        self.name = name
        self.typespec = typespec

class TypeSpec(Node):
    def __init__(self, loc, spec, state=None):
        Node.__init__(self, loc)
        self.spec = spec
        self.state = state

    def basename(self):
        return self.spec.baseid

    def isActor(self):
        return self.state is not None

    def __str__(self):  return str(self.spec)

class QualifiedId:              # FIXME inherit from node?
    def __init__(self, loc, baseid, quals=[ ]):
        assert isinstance(baseid, str)
        for qual in quals: assert isinstance(qual, str)

        self.loc = loc
        self.baseid = baseid
        self.quals = quals

    def qualify(self, id):
        self.quals.append(self.baseid)
        self.baseid = id

    def __str__(self):
        if 0 == len(self.quals):
            return self.baseid
        return '::'.join(self.quals) +'::'+ self.baseid

# added by type checking passes
class Decl(Node):
    def __init__(self, loc):
        Node.__init__(self, loc)
        self.progname = None    # what the programmer typed, if relevant
        self.shortname = None   # shortest way to refer to this decl
        self.fullname = None    # full way to refer to this decl
        self.loc = loc
        self.type = None
        self.scope = None
