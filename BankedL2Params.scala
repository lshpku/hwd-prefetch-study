// See LICENSE.SiFive for license details.

package freechips.rocketchip.subsystem

import chisel3._
import chisel3.util._
import freechips.rocketchip.config._
import freechips.rocketchip.devices.tilelink.BuiltInDevices
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.interrupts._
import freechips.rocketchip.tilelink._
import freechips.rocketchip.util._
import CoherenceManagerWrapper._

/** Global cache coherence granularity, which applies to all caches, for now. */
case object CacheBlockBytes extends Field[Int](64)

/** L2 Broadcast Hub configuration */
case object BroadcastKey extends Field(BroadcastParams())

case class BroadcastParams(
  nTrackers:      Int     = 4,
  bufferless:     Boolean = false,
  controlAddress: Option[BigInt] = None,
  filterFactory:  TLBroadcast.ProbeFilterFactory = BroadcastFilter.factory)

/** L2 memory subsystem configuration */
case object BankedL2Key extends Field(BankedL2Params())

case class BankedL2Params(
  nBanks: Int = 1,
  coherenceManager: CoherenceManagerInstantiationFn = broadcastManager
) {
  require (isPow2(nBanks) || nBanks == 0)
}

case class CoherenceManagerWrapperParams(
    blockBytes: Int,
    beatBytes: Int,
    nBanks: Int,
    name: String,
    dtsFrequency: Option[BigInt] = None)
  (val coherenceManager: CoherenceManagerInstantiationFn)
  extends HasTLBusParams 
  with TLBusWrapperInstantiationLike
{
  def instantiate(context: HasTileLinkLocations, loc: Location[TLBusWrapper])(implicit p: Parameters): CoherenceManagerWrapper = {
    val cmWrapper = LazyModule(new CoherenceManagerWrapper(this, context))
    cmWrapper.suggestName(loc.name + "_wrapper")
    cmWrapper.halt.foreach { context.anyLocationMap += loc.halt(_) }
    context.tlBusWrapperLocationMap += (loc -> cmWrapper)
    cmWrapper
  }
}

class CoherenceManagerWrapper(params: CoherenceManagerWrapperParams, context: HasTileLinkLocations)(implicit p: Parameters) extends TLBusWrapper(params, params.name) {
  val (tempIn, tempOut, halt) = params.coherenceManager(context)

  private val coherent_jbar = LazyModule(new TLJbar)
  def busView: TLEdge = coherent_jbar.node.edges.out.head
  val inwardNode = tempIn :*= coherent_jbar.node
  val builtInDevices = BuiltInDevices.none
  val prefixNode = None
  println ("params.nBanks", params.nBanks)
  println ("params.blockBytes", params.blockBytes)

  private def banked(node: TLOutwardNode): TLOutwardNode =
    if (params.nBanks == 0) node else { TLTempNode() :=* BankBinder(params.nBanks, params.blockBytes) :*= node }

  val outwardNode = TLBuffer(BufferParams.none, DelayerParams(100, 8)) :=* banked(tempOut)
}

object CoherenceManagerWrapper {
  type CoherenceManagerInstantiationFn = HasTileLinkLocations => (TLInwardNode, TLOutwardNode, Option[IntOutwardNode])

  def broadcastManagerFn(
    name: String,
    location: HierarchicalLocation,
    controlPortsSlaveWhere: TLBusWrapperLocation
  ): CoherenceManagerInstantiationFn = { context =>
    implicit val p = context.p
    val cbus = context.locateTLBusWrapper(controlPortsSlaveWhere)

    val BroadcastParams(nTrackers, bufferless, controlAddress, filterFactory) = p(BroadcastKey)
    val bh = LazyModule(new TLBroadcast(TLBroadcastParams(
      lineBytes     = p(CacheBlockBytes),
      numTrackers   = nTrackers,
      bufferless    = bufferless,
      control       = controlAddress.map(x => TLBroadcastControlParams(AddressSet(x, 0xfff), cbus.beatBytes)),
      filterFactory = filterFactory)))
    bh.suggestName(name)

    bh.controlNode.foreach { _ := cbus.coupleTo(s"${name}_ctrl") { TLBuffer(1) := TLFragmenter(cbus) := _ } }
    bh.intNode.foreach { context.ibus.fromSync := _ }

    (bh.node, bh.node, None)
  }

  val broadcastManager = broadcastManagerFn("broadcast", InSystem, CBUS)

  val incoherentManager: CoherenceManagerInstantiationFn = { _ =>
    val node = TLNameNode("no_coherence_manager")
    (node, node, None)
  }
}

class TokenDelayer[T <: Data](gen: T, delay: Int, entries: Int) extends Module {
  val io = IO(new Bundle {
    val enq = Flipped(Decoupled(gen))
    val deq = Decoupled(gen)
  })
  require(delay > 1, "Use a normal Queue if you want a delay of 1")

  class DebugData extends Bundle {
    val data = gen.cloneType
    val debug_id = UInt(log2Ceil(delay + 1).W)
  }
  val debug_id = RegInit(0.U(log2Ceil(delay + 1).W))
  val cycle = freechips.rocketchip.util.WideCounter(32).value

  val aging = RegInit(0.U((delay - 1).W))
  val tokens = RegInit(0.U(log2Ceil(entries + 1).W))
  val queue = Module(new Queue(new DebugData, entries))
  aging := aging << 1
  when (aging(delay - 2) === 1.U) {
    assert (tokens < entries.U, "Tokens overflow")
    tokens := tokens + 1.U
  }

  // enqueue logic
  io.enq.ready := queue.io.enq.ready
  queue.io.enq.valid := io.enq.valid
  queue.io.enq.bits.data := io.enq.bits
  queue.io.enq.bits.debug_id := debug_id
  when (io.enq.fire) {
    aging := (aging << 1).asUInt | 1.U
    when (debug_id =/= delay.U) {
      debug_id := debug_id + 1.U
    } .otherwise {
      debug_id := 0.U
    }
  }

  // dequeue logic
  io.deq.valid := false.B
  io.deq.bits := queue.io.deq.bits.data
  queue.io.deq.ready := false.B
  when (tokens > 0.U) {
    io.deq.valid := queue.io.deq.valid
    queue.io.deq.ready := io.deq.ready
    when (io.deq.fire) {
      when (aging(delay - 2) === 0.U) {
        tokens := tokens - 1.U
      } .otherwise {
        tokens := tokens
      }
    }
  }

  assert (!(queue.io.deq.valid && tokens === 0.U && aging === 0.U), "Tokens leaked")
}

class DelayerParams(delay: Int, entries: Int) extends BufferParams(delay, false, false) {
  require (delay >= 0, "Delay must be >= 0")
  override def apply[T <: Data](x: DecoupledIO[T]): DecoupledIO[T] = {
    if (delay > 1) {
      val delayer = Module(new TokenDelayer(chiselTypeOf(x.bits), delay, entries))
      println("TokenDelayer", delay, entries)
      delayer.io.enq <> x
      delayer.io.deq
    } else if (delay > 0) {
      Queue(x)
    } else {
      x
    }
  }
}

object DelayerParams
{
  def apply(delay: Int): DelayerParams = apply(delay, delay)
  def apply(delay: Int, entries: Int): DelayerParams = new DelayerParams(delay, entries)
}
