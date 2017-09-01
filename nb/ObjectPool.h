//==============================================================================
//
//  ObjectPool.h
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#ifndef OBJECTPOOL_H_INCLUDED
#define OBJECTPOOL_H_INCLUDED

#include "Protected.h"
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include "NbTypes.h"
#include "Q1Way.h"
#include "RegCell.h"
#include "SysTypes.h"

namespace NodeBase
{
   struct ObjectBlock;
   class ObjectPoolStats;
   class Pooled;
}

//------------------------------------------------------------------------------

namespace NodeBase
{
//  An object pool allocates blocks during system initialization.  The blocks
//  are placed on a free queue and are dequeued at run-time to provide memory
//  for instantiating PooledObjects.  To simplify the engineering of pool
//  sizes, all objects subclassed from a common application framework class
//  should draw their blocks from the same pool.
//
class ObjectPool : public Protected
{
   friend class ObjectPoolRegistry;
   friend class ObjectPoolSizeCfg;
   friend class Registry< ObjectPool >;
public:
   //> Highest valid object pool identifier.
   //
   static const ObjectPoolId MaxId = 63;

   //> The maximum number of segments in an object pool.
   //
   static const size_t MaxSegments = 256;

   //  Blocks for pooled objects are allocated in segments of 1K blocks.
   //
   static const size_t ObjectsPerSegment = 1024;

   //  Used in a shift (>>) operation to find the segment to which a block
   //  belongs.
   //
   static const size_t ObjectsPerSegmentLog2 = 10;

   //  Allows "Bid" to refer to an object block identifier in this class
   //  hierarchy.  The first block in a pool is #1.
   //
   typedef PooledObjectId Bid;

   //  Highest valid object block identifier.
   //
   static const Bid MaxBid = MaxSegments << ObjectsPerSegmentLog2;

   //> Highest valid sequence number.  Sequence numbers distinguish a block's
   //  incarnations.  Their use is mandatory when a Pooled object can receive
   //  interprocessor messages, as they allow stale messages to be detected
   //  and discarded.
   //
   static const PooledObjectSeqNo MaxSeqNo = UINT8_MAX;

   //  Returns the pool's name.
   //
   const std::string& Name() const { return name_; }

   //  Returns the pool's identifier.
   //
   ObjectPoolId Pid() const { return ObjectPoolId(pid_.GetId()); }

   //  Creates or expands the object pool so that it contains the target
   //  number of segments.  A pool's size can be increased at run time,
   //  but it can only be decreased during a restart.
   //
   bool AllocBlocks();

   //  Allocates a block from the free queue.  SIZE specifies the size
   //  of the object to be constructed within the block.
   //
   virtual Pooled* DeqBlock(size_t size);

   //  Returns an object's block to the free queue.  DELETED is set if
   //  the block was freed by an implementation of operator delete.
   //
   virtual void EnqBlock(Pooled* obj, bool deleted);

   //  Returns the pool's first in-use block and updates the iterator
   //  BID to reference it.
   //
   Pooled* FirstUsed(Bid& bid) const;

   //  Returns the pool's next in-use block after the one referenced
   //  by the iterator BID.
   //
   Pooled* NextUsed(Bid& bid) const;

   //  Converts OBJ to an object block identifier.  Returns -1 if OBJ does
   //  not reference a block in the pool or if inUseOnly is true and the
   //  block is not currently unassigned.
   //
   PooledObjectId ObjBid(const Pooled* obj, bool inUseOnly) const;

   //  Returns the pool to which an object belongs.
   //
   static ObjectPoolId ObjPid(const Pooled* obj);

   //  Returns an object's sequence number.
   //
   static PooledObjectSeqNo ObjSeq(const Pooled* obj);

   //  Returns a pointer to the object identified by BID.  Returns nullptr if
   //  BID is invalid or the block identified by BID is currently unassigned.
   //
   Pooled* BidToObj(Bid bid) const;

   //  Returns the total number of blocks on the free queue.
   //
   size_t AvailCount() const { return availCount_; }

   //  Returns the total number of blocks currently in use.
   //
   size_t InUseCount() const;

   //  Returns the minimum number of available blocks during the
   //  current statistics interval.
   //
   size_t LowAvailCount() const;

   //  Returns the number of allocation failures during the current
   //  statistics interval.
   //
   size_t FailCount() const;

   //  Returns the number of allocations.
   //
   size_t AllocCount() const;

   //  Returns the number of deallocations.
   //
   size_t FreeCount() const;

   //  Returns the type of memory used by the pool's blocks.
   //
   MemoryType BlockType() const { return type_; }

   //  Displays statistics.  May be overridden to include pool-specific
   //  statistics, but the base class version must be invoked.
   //
   virtual void DisplayStats(std::ostream& stream) const;

   //  Displays in-use blocks.  Returns false if no block were in use.
   //
   bool DisplayUsed(std::ostream& stream,
      const std::string& prefix, const Flags& options) const;

   //  Corrupts the Nth link on the free queue for testing (0 = queue header).
   //  Returns false if the queue contained less than N elements.
   //
   bool Corrupt(size_t n);

   //  Returns the offset to pid_.
   //
   static ptrdiff_t CellDiff();

   //  Overridden for restarts.
   //
   virtual void Startup(RestartLevel level) override;

   //  Overridden for restarts.
   //
   virtual void Shutdown(RestartLevel level) override;

   //  Overridden to display member variables.
   //
   virtual void Display(std::ostream& stream,
      const std::string& prefix, const Flags& options) const override;

   //  Overridden for patching.
   //
   virtual void Patch(sel_t selector, void* arguments) override;
protected:
   //  Defines a pool, identified by NAME and PID, that allocates blocks of
   //  TYPE, with a size of nBytes.  Protected because this class is virtual.
   //
   ObjectPool(ObjectPoolId pid, MemoryType type,
      size_t nBytes, const std::string& name);

   //  Frees all blocks.  Protected because subclasses should be singletons.
   //
   virtual ~ObjectPool();
private:
   //  Used in a mask (&) operation to find a block's offset in its segment.
   //
   static const size_t ObjectSecondIndexMask = ObjectsPerSegment - 1;

   //  Returns the first block in the pool and updates the iterator BID
   //  to reference it.
   //
   ObjectBlock* First(Bid& bid) const;

   //  Returns the next block in the pool after the one referenced by the
   //  iterator BID.
   //
   ObjectBlock* Next(Bid& bid) const;

   //  Maps the block identifier BID to the first and second level indices
   //  that access it in blocks_.  Returns false if BID does not reference
   //  a block in the pool.
   //
   bool BidToIndices(Bid bid, size_t& i, size_t& j) const;

   //  Maps the first and second level indices, I and J, to the block
   //  identifier BID.  Returns false if I or J is invalid.
   //
   bool IndicesToBid(size_t i, size_t j, Bid& bid) const;

   //  Marks all blocks as orphaned and audits the free queue for sanity,
   //  unmarking its blocks so that they will not be recovered.
   //
   void AuditFreeq();

   //  Recovers orphaned blocks after AuditFreeq and ClaimBlocks have
   //  marked all in-use and free blocks.
   //
   void RecoverBlocks();

   //  Accesses the header that resides above OBJ.
   //
   static ObjectBlock* ObjToBlock(const Pooled* obj);

   //  Overridden to prohibit copying.
   //
   ObjectPool(const ObjectPool& that);
   void operator=(const ObjectPool& that);

   //  The pool's identifier.
   //
   RegCell pid_;

   //  The pool's name.
   //
   const std::string name_;  //r

   //  The string "NumOf" + name_, which identifies (in the element
   //  configuration file) the parameter that determines the number
   //  of blocks in the pool.
   //
   const std::string key_;  //r

   //  The type of memory used for blocks in the pool.
   //
   const MemoryType type_;

   //  The size of each block in bytes, rounded up for alignment purposes.
   //
   size_t blockSize_;

   //  The increment used when iterating through the blocks in a segment.
   //
   size_t segIncr_;

   //  The size of a segment in bytes (and therefore the first out-of-bounds
   //  index when iterating through blocks in the segment).
   //
   size_t segSize_;

   //  The current number of segments in the pool.
   //
   word currSegments_;

   //  The desired number of segments in the pool.
   //
   word targSegments_;

   //  The configuration parameter for the number of segments in the pool.
   //
   CfgIntParmPtr cfgSegments_;

   //  All of the blocks in the pool, allocated in segments.
   //
   uword* blocks_[MaxSegments];

   //  The queue of available blocks.
   //
   Q1Way< Pooled > freeq_;

   //  The number of blocks in freeq_.
   //
   size_t availCount_;

   //  Used to detect a corrupt queue header when auditing freeq_.
   //
   bool corruptQHead_;

   //  The pool's statistics.
   //
   std::unique_ptr< ObjectPoolStats > stats_;

   //> The number of audit cycles over which a block must be unclaimed
   //  before it is recovered.
   //
   static const uint8_t OrphanThreshold;
};
}
#endif