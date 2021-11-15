#include "zg/private/PZGConstants.h"

namespace zg_private
{

const String PZG_PEER_NAME_USER_MESSAGE        = "ums";
const String PZG_PEER_NAME_DATABASE_ID         = "dbi";
const String PZG_PEER_NAME_DATABASE_UPDATE     = "dbu";
const String PZG_PEER_NAME_DATABASE_UPDATE_ID  = "dui";
const String PZG_PEER_NAME_TEXT                = "txt";
const String PZG_PEER_NAME_CHECKSUM_MISMATCH   = "chk";
const String PZG_PEER_NAME_BACK_ORDER          = "ubok";

/** Return a brief description of the peerInfo data that we can display easily on a single line */
String PeerInfoToString(const ConstMessageRef & peerInfo)
{  
   if (peerInfo() == NULL) return("No peer info");
   
   const Message & pm = *peerInfo();
   String ret;
   for (MessageFieldNameIterator fnIter(pm); fnIter.HasData(); fnIter++)
   {  
      const String & fn = fnIter.GetFieldName();
      uint32 fieldTypeCode;
      if (pm.GetInfo(fn, &fieldTypeCode).IsOK())
      {  
         switch(fieldTypeCode)
         { 
           case B_BOOL_TYPE:   ret += String(" %1=%2").Arg(fn).Arg(pm.GetBool(fn)?"true":"false"); break;
           case B_DOUBLE_TYPE: ret += String(" %1=%2").Arg(fn).Arg(pm.GetDouble(fn)); break;
           case B_FLOAT_TYPE:  ret += String(" %1=%2").Arg(fn).Arg(pm.GetFloat(fn));  break;
           case B_INT64_TYPE:  ret += String(" %1=%2").Arg(fn).Arg(pm.GetInt64(fn));  break;
           case B_INT32_TYPE:  ret += String(" %1=%2").Arg(fn).Arg(pm.GetInt32(fn));  break;
           case B_INT16_TYPE:  ret += String(" %1=%2").Arg(fn).Arg(pm.GetInt16(fn));  break;
           case B_INT8_TYPE:   ret += String(" %1=%2").Arg(fn).Arg((int)pm.GetInt8(fn)); break;
           case B_STRING_TYPE: ret += String(" %1=%2").Arg(fn).Arg(pm.GetString(fn)); break;
           default:   /* do nothing */ break;
         }
      }
   } 
   return ret.Trim();
}

};  // end namespace zg_private
