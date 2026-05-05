
#include "precompiled.h"
#include "../idLib/preylib.h"

static const int HH_MAX_BEAM_NODES = 32;

enum {
    HH_BEAMCMD_SPLINE_LINEAR_TO_TARGET = 0,
    HH_BEAMCMD_SPLINE_ARC_TO_TARGET = 1,
    HH_BEAMCMD_SPLINE_ADD = 2,
    HH_BEAMCMD_SPLINE_ADD_SIN = 3,
    HH_BEAMCMD_SPLINE_ADD_SIN_TIME = 4,
    HH_BEAMCMD_SPLINE_ADD_SIN_TIME_SCALED = 5,
    HH_BEAMCMD_CONVERT_SPLINE_TO_NODES = 6,
    HH_BEAMCMD_NODE_LINEAR_TO_TARGET = 7,
    HH_BEAMCMD_NODE_ELECTRIC = 8
};

static void HH_InitBeamCmd(beamCmd_t& cmd, const int command, const int index) {
    int* raw = reinterpret_cast<int*>(&cmd);

    /*
        The dump initializes all unused dwords after command to the current
        command index before overwriting the fields that a command actually uses.
    */
    raw[0] = command;
    raw[1] = index;
    raw[2] = index;
    raw[3] = index;
    raw[4] = index;
    raw[5] = index;
    raw[6] = index;
    raw[7] = index;
}

static beamCmd_t& HH_AllocBeamCmd(idList<beamCmd_t>& list, const int command) {
    const int index = list.Num();

    beamCmd_t& cmd = list.Alloc();
    HH_InitBeamCmd(cmd, command, index);

    return cmd;
}

static void HH_SetBeamCmdIntArg(beamCmd_t& cmd, const int value) {
    reinterpret_cast<int*>(&cmd)[1] = value;
}

static void HH_SetBeamCmdVecA(beamCmd_t& cmd, const idVec3& value) {
    float* raw = reinterpret_cast<float*>(&cmd);

    raw[2] = value.x;
    raw[3] = value.y;
    raw[4] = value.z;
}

static void HH_SetBeamCmdVecB(beamCmd_t& cmd, const idVec3& value) {
    float* raw = reinterpret_cast<float*>(&cmd);

    raw[5] = value.x;
    raw[6] = value.y;
    raw[7] = value.z;
}

static idVec3 HH_ParseBeamVec3(idLexer& src) {
    idVec3 value;

    src.ExpectTokenString("(");
    value.x = src.ParseFloat();
    src.ExpectTokenString(",");
    value.y = src.ParseFloat();
    src.ExpectTokenString(",");
    value.z = src.ParseFloat();
    src.ExpectTokenString(")");

    return value;
}

/*
================
hhDeclBeam::hhDeclBeam
================
*/
hhDeclBeam::hhDeclBeam() {
    numNodes = 0;
    numBeams = 0;

    for (int i = 0; i < MAX_BEAMS; i++) {
        thickness[i] = 0.0f;

        bFlat[i] = false;
        bTaperEndPoints[i] = false;

        shader[i] = NULL;

        quadShader[i][0] = NULL;
        quadShader[i][1] = NULL;

        quadSize[i][0] = 0.0f;
        quadSize[i][1] = 0.0f;

        cmds[i].Clear();
        cmds[i].SetGranularity(16);
    }
}

/*
================
hhDeclBeam::DefaultDefinition
================
*/
const char* hhDeclBeam::DefaultDefinition() const {
    return "{ numNodes 2 numBeams 1 _white { NodeLinearToTarget } }";
}

/*
================
hhDeclBeam::FreeData
================
*/
void hhDeclBeam::FreeData() {
    numNodes = 0;
    numBeams = 0;

    for (int i = 0; i < MAX_BEAMS; i++) {
        thickness[i] = 0.0f;

        bFlat[i] = false;
        bTaperEndPoints[i] = false;

        shader[i] = NULL;

        quadShader[i][0] = NULL;
        quadShader[i][1] = NULL;

        quadSize[i][0] = 0.0f;
        quadSize[i][1] = 0.0f;

        cmds[i].Clear();
        cmds[i].SetGranularity(16);
    }
}

/*
================
hhDeclBeam::Parse
================
*/
bool hhDeclBeam::Parse(const char* text, const int textLength) {
    idLexer src;
    idToken token;

    src.LoadMemory(text, textLength, GetFileName(), GetLineNum());
    src.SetFlags(DECL_LEXER_FLAGS);

    src.SkipUntilString("{");

    src.ExpectTokenString("numNodes");
    numNodes = src.ParseInt();
    if (numNodes > HH_MAX_BEAM_NODES) {
        numNodes = HH_MAX_BEAM_NODES;
    }

    src.ExpectTokenString("numBeams");
    numBeams = src.ParseInt();
    if (numBeams > MAX_BEAMS) {
        numBeams = MAX_BEAMS;
    }

    /*
        The dump only treats exactly zero as a parse error. A negative value
        falls through and returns true without parsing any beam blocks.
    */
    if (numBeams == 0) {
        src.Warning("Beam decl '%s' had a parse error", GetName());
        return false;
    }

    if (numBeams <= 0) {
        return true;
    }

    for (int beamNum = 0; beamNum < numBeams; beamNum++) {
        thickness[beamNum] = 1.0f;

        bFlat[beamNum] = false;
        bTaperEndPoints[beamNum] = false;

        quadShader[beamNum][0] = NULL;
        quadShader[beamNum][1] = NULL;

        quadSize[beamNum][0] = 1.0f;
        quadSize[beamNum][1] = 1.0f;

        cmds[beamNum].Clear();
        cmds[beamNum].SetGranularity(16);

        /*
            Per beam block format:

                <materialName> {
                    ...
                }
        */
        if (!src.ReadToken(&token)) {
            src.Warning("Beam decl '%s' had a parse error", GetName());
            return false;
        }

        shader[beamNum] = declManager->FindMaterial(token.c_str());

        src.ExpectTokenString("{");

        while (src.ReadToken(&token)) {
            if (!token.Icmp("}")) {
                break;
            }

            /*
            ===================================================================
                Beam properties
            ===================================================================
            */

            if (!token.Icmp("thickness")) {
                thickness[beamNum] = src.ParseFloat();
                continue;
            }

            if (!token.Icmp("taperEnds")) {
                bTaperEndPoints[beamNum] = (src.ParseInt() != 0);
                continue;
            }

            if (!token.Icmp("flat")) {
                bFlat[beamNum] = (src.ParseInt() != 0);
                continue;
            }

            if (!token.Icmp("quadOriginShader")) {
                src.ReadToken(&token);
                quadShader[beamNum][0] = declManager->FindMaterial(token.c_str());
                continue;
            }

            if (!token.Icmp("quadEndShader")) {
                src.ReadToken(&token);
                quadShader[beamNum][1] = declManager->FindMaterial(token.c_str());
                continue;
            }

            if (!token.Icmp("quadOriginSize")) {
                quadSize[beamNum][0] = src.ParseFloat();
                continue;
            }

            if (!token.Icmp("quadEndSize")) {
                quadSize[beamNum][1] = src.ParseFloat();
                continue;
            }

            if (!token.Icmp("SplineLinearToTarget")) {
                HH_AllocBeamCmd(cmds[beamNum], HH_BEAMCMD_SPLINE_LINEAR_TO_TARGET);
                continue;
            }

            if (!token.Icmp("SplineArcToTarget")) {
                HH_AllocBeamCmd(cmds[beamNum], HH_BEAMCMD_SPLINE_ARC_TO_TARGET);
                continue;
            }

            if (!token.Icmp("SplineAdd")) {
                const int intArg = src.ParseInt();
                const idVec3 vecA = HH_ParseBeamVec3(src);

                beamCmd_t& cmd = HH_AllocBeamCmd(cmds[beamNum], HH_BEAMCMD_SPLINE_ADD);
                HH_SetBeamCmdIntArg(cmd, intArg);
                HH_SetBeamCmdVecA(cmd, vecA);
                continue;
            }

            if (!token.Icmp("SplineAddSin")) {
                const int intArg = src.ParseInt();

                /*
                    The dump parses the first vector, then the second vector,
                    but stores the second vector at +08 and the first at +20.
                */
                const idVec3 vecB = HH_ParseBeamVec3(src);
                const idVec3 vecA = HH_ParseBeamVec3(src);

                beamCmd_t& cmd = HH_AllocBeamCmd(cmds[beamNum], HH_BEAMCMD_SPLINE_ADD_SIN);
                HH_SetBeamCmdIntArg(cmd, intArg);
                HH_SetBeamCmdVecA(cmd, vecA);
                HH_SetBeamCmdVecB(cmd, vecB);
                continue;
            }

            if (!token.Icmp("SplineAddSinTime")) {
                const int intArg = src.ParseInt();

                const idVec3 vecB = HH_ParseBeamVec3(src);
                const idVec3 vecA = HH_ParseBeamVec3(src);

                beamCmd_t& cmd = HH_AllocBeamCmd(cmds[beamNum], HH_BEAMCMD_SPLINE_ADD_SIN_TIME);
                HH_SetBeamCmdIntArg(cmd, intArg);
                HH_SetBeamCmdVecA(cmd, vecA);
                HH_SetBeamCmdVecB(cmd, vecB);
                continue;
            }

            if (!token.Icmp("SplineAddSinTimeScaled")) {
                const int intArg = src.ParseInt();

                const idVec3 vecB = HH_ParseBeamVec3(src);
                const idVec3 vecA = HH_ParseBeamVec3(src);

                beamCmd_t& cmd = HH_AllocBeamCmd(cmds[beamNum], HH_BEAMCMD_SPLINE_ADD_SIN_TIME_SCALED);
                HH_SetBeamCmdIntArg(cmd, intArg);
                HH_SetBeamCmdVecA(cmd, vecA);
                HH_SetBeamCmdVecB(cmd, vecB);
                continue;
            }

            if (!token.Icmp("ConvertSplineToNodes")) {
                HH_AllocBeamCmd(cmds[beamNum], HH_BEAMCMD_CONVERT_SPLINE_TO_NODES);
                continue;
            }

            if (!token.Icmp("NodeLinearToTarget")) {
                HH_AllocBeamCmd(cmds[beamNum], HH_BEAMCMD_NODE_LINEAR_TO_TARGET);
                continue;
            }

            if (!token.Icmp("NodeElectric")) {
                const idVec3 vecA = HH_ParseBeamVec3(src);

                beamCmd_t& cmd = HH_AllocBeamCmd(cmds[beamNum], HH_BEAMCMD_NODE_ELECTRIC);
                HH_SetBeamCmdVecA(cmd, vecA);
                continue;
            }

            common->FatalError("Unknown token %s\n", token.c_str());
        }
    }

    return true;
}