// automatically generated by the FlatBuffers compiler, do not modify

package MyGame.Example;

import java.nio.*;
import java.lang.*;
import java.util.*;
import com.google.flatbuffers.*;

@SuppressWarnings("unused")
public final class Referrable extends Table {
  public static Referrable getRootAsReferrable(ByteBuffer _bb) { return getRootAsReferrable(_bb, new Referrable()); }
  public static Referrable getRootAsReferrable(ByteBuffer _bb, Referrable obj) { _bb.order(ByteOrder.LITTLE_ENDIAN); return (obj.__assign(_bb.getInt(_bb.position()) + _bb.position(), _bb)); }
  public void __init(int _i, ByteBuffer _bb) { bb_pos = _i; bb = _bb; vtable_start = bb_pos - bb.getInt(bb_pos); vtable_size = bb.getShort(vtable_start); }
  public Referrable __assign(int _i, ByteBuffer _bb) { __init(_i, _bb); return this; }

  public long id() { int o = __offset(4); return o != 0 ? bb.getLong(o + bb_pos) : 0L; }
  public boolean mutateId(long id) { int o = __offset(4); if (o != 0) { bb.putLong(o + bb_pos, id); return true; } else { return false; } }

  public static int createReferrable(FlatBufferBuilder builder,
      long id) {
    builder.startObject(1);
    Referrable.addId(builder, id);
    return Referrable.endReferrable(builder);
  }

  public static void startReferrable(FlatBufferBuilder builder) { builder.startObject(1); }
  public static void addId(FlatBufferBuilder builder, long id) { builder.addLong(0, id, 0L); }
  public static int endReferrable(FlatBufferBuilder builder) {
    int o = builder.endObject();
    return o;
  }

  @Override
  protected int keysCompare(Integer o1, Integer o2, ByteBuffer _bb) {
    long val_1 = _bb.getLong(__offset(4, o1, _bb));
    long val_2 = _bb.getLong(__offset(4, o2, _bb));
    return val_1 > val_2 ? 1 : val_1 < val_2 ? -1 : 0;
  }

  public static Referrable __lookup_by_key(Referrable obj, int vectorLocation, long key, ByteBuffer bb) {
    int span = bb.getInt(vectorLocation - 4);
    int start = 0;
    while (span != 0) {
      int middle = span / 2;
      int tableOffset = __indirect(vectorLocation + 4 * (start + middle), bb);
      long val = bb.getLong(__offset(4, bb.capacity() - tableOffset, bb));
      int comp = val > key ? 1 : val < key ? -1 : 0;
      if (comp > 0) {
        span = middle;
      } else if (comp < 0) {
        middle++;
        start += middle;
        span -= middle;
      } else {
        return (obj == null ? new Referrable() : obj).__assign(tableOffset, bb);
      }
    }
    return null;
  }
}

