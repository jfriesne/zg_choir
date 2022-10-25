#ifndef ZGPeerID_h
#define ZGPeerID_h

#include <string.h>  // for strchr()
#include "support/Flattenable.h"
#include "util/String.h"
#include "zg/ZGNameSpace.h"

namespace zg
{

enum {ZG_PEER_ID_TYPE = 2053597540}; /**< 'zgid' -- the type code of the ZGPeerID class */

/** A ZGPeerID is a 128-bit integer that uniquely represents a particular peer in a ZG system.
  * A peer's ZGPeerID is assigned to at startup, and will never change over the course of the peer's lifetime.
  * It will be different for each ZG peer process that is started.
  */
class ZGPeerID : public PseudoFlattenable
{
public:
   /** Default constructor -- sets the ZGPeerID to its invalid state (all zeroes) */
   ZGPeerID() : _highBits(0), _lowBits(0) {/* empty */}

   /** Explicit constructor
     * @param highBits the upper 64 bits of the ZGPeerID
     * @param lowBits the lower 64 bits of the ZGPeerID
     */
   ZGPeerID(uint64 highBits, uint64 lowBits) : _highBits(highBits), _lowBits(lowBits) {/* empty */}

   /** Copy constructor
     * @param rhs the ZGPeerID to make this a copy of
     */
   ZGPeerID(const ZGPeerID & rhs) : _highBits(rhs._highBits), _lowBits(rhs._lowBits) {/* empty */}

   /** Equality operator; returns true iff this ZGPeerID is equal to (rhs)
     * @param rhs The ZGPeerID to compare to
     */
   bool operator == (const ZGPeerID & rhs) const {return ((_highBits == rhs._highBits)&&(_lowBits == rhs._lowBits));}

   /** Inequality operator; returns true iff this ZGPeerID is not equal to (rhs)
     * @param rhs The ZGPeerID to compare to
     */
   bool operator != (const ZGPeerID & rhs) const {return !(*this==rhs);}

   /** Comparison operator; returns true iff this ZGPeerID is less than (rhs)
     * @param rhs The ZGPeerID to compare to
     */
   bool operator <  (const ZGPeerID & rhs) const {return ((_highBits < rhs._highBits)||((_highBits == rhs._highBits)&&(_lowBits < rhs._lowBits)));}

   /** Comparison operator; returns true iff this ZGPeerID is greater than or equal to (rhs)
     * @param rhs The ZGPeerID to compare to
     */
   bool operator >= (const ZGPeerID & rhs) const {return !(*this<rhs);}

   /** Comparison operator; returns true iff this ZGPeerID is greater than (rhs)
     * @param rhs The ZGPeerID to compare to
     */
   bool operator >  (const ZGPeerID & rhs) const {return ((_highBits > rhs._highBits)||((_highBits == rhs._highBits)&&(_lowBits > rhs._lowBits)));}

   /** Comparison operator; returns true iff this ZGPeerID is less than or equal to (rhs)
     * @param rhs The ZGPeerID to compare to
     */
   bool operator <= (const ZGPeerID & rhs) const {return !(*this>rhs);}

   /** Assignment operator.  Sets (*this) to be the same as (rhs)
     * @param rhs The ZGPeerID to copy the state of
     */
   ZGPeerID & operator = (const ZGPeerID & rhs) {_highBits = rhs._highBits; _lowBits = rhs._lowBits; return *this;}

   /** Returns true iff the ZGPeerID has any non-zero bits in it.  (An all-zero ZGPeerID is considered an invalid/null ID) */
   bool IsValid() const {return ((_highBits != 0)||(_lowBits != 0));}

   /** Sets this peer ID from the specified String representation (in the format used by ToString()), or to zero if the string isn't parsed
     * @param s A human-readable string (e.g. "123A:432B") that we will set this ZGPeerID's state from
     */
   void FromString(const String & s)
   {
      _highBits = _lowBits = 0;

      const char * colon = strchr(s(), ':');
      if (colon)
      {
         _highBits = Atoxll(s());
         _lowBits  = Atoxll(colon+1);
      }
   }

   /** Returns a String representation of this peer ID (e.g. "123A:432B") */
   String ToString() const
   {
      char buf[256];
      muscleSprintf(buf, XINT64_FORMAT_SPEC ":" XINT64_FORMAT_SPEC, _highBits, _lowBits);
      return buf;
   }

   /** Part of the Flattenable pseudo-interface:  Returns true */
   static MUSCLE_CONSTEXPR bool IsFixedSize() {return true;}

   /** Part of the Flattenable pseudo-interface:  Returns ZG_PEER_ID_TYPE */
   static MUSCLE_CONSTEXPR uint32 TypeCode() {return ZG_PEER_ID_TYPE;}

   /** Returns true iff (tc) equals ZG_PEER_ID_TYPE */
   static MUSCLE_CONSTEXPR bool AllowsTypeCode(uint32 tc) {return (TypeCode()==tc);}

   /** Part of the Flattenable pseudo-interface:  Returns 2*sizeof(uint64) */
   static MUSCLE_CONSTEXPR uint32 FlattenedSize() {return 2*sizeof(uint64);}

   /** Returns a 32-bit checksum for this object. */
   uint32 CalculateChecksum() const {return CalculateChecksumForUint64(_highBits) + (3*CalculateChecksumForUint64(_lowBits));}

   /** Copies this object into an endian-neutral flattened buffer.
    *  @param flat the DataFlattener to use to write out bytes
    */
   void Flatten(DataFlattener flat) const
   {
      flat.WriteInt64(_highBits);
      flat.WriteInt64(_lowBits);
   }

   /** Restores this object from an endian-neutral flattened buffer.
    *  @param unflat the DataUnflattener to read bytes from.
    *  @return B_NO_ERROR on success, an error code on failure (size was too small)
    */
   status_t Unflatten(DataUnflattener & unflat)
   {
      _highBits = unflat.ReadInt64();
      _lowBits  = unflat.ReadInt64();
      return unflat.GetStatus();
   }

   /** This is implemented so that if ZGPeerID is used as the key in a Hashtable, the HashCode() method will be
     * selected by the AutoChooseHashFunctor template logic, instead of the PODHashFunctor.
     */
   uint32 HashCode() const {return CalculateChecksum();}

private:
   uint64 _highBits;
   uint64 _lowBits;
};

};  // end namespace zg

#endif
