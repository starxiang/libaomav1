// automatically generated by the FlatBuffers compiler, do not modify

package MyGame;

import java.nio.*;
import java.lang.*;
import java.util.*;
import com.google.flatbuffers.*;

@SuppressWarnings("unused")
public final class InParentNamespace extends Table {
  public static InParentNamespace getRootAsInParentNamespace(ByteBuffer _bb) { return getRootAsInParentNamespace(_bb, new InParentNamespace()); }
  public static InParentNamespace getRootAsInParentNamespace(ByteBuffer _bb, InParentNamespace obj) { _bb.order(ByteOrder.LITTLE_ENDIAN); return (obj.__assign(_bb.getInt(_bb.position()) + _bb.position(), _bb)); }
  public void __init(int _i, ByteBuffer _bb) { bb_pos = _i; bb = _bb; vtable_start = bb_pos - bb.getInt(bb_pos); vtable_size = bb.getShort(vtable_start); }
  public InParentNamespace __assign(int _i, ByteBuffer _bb) { __init(_i, _bb); return this; }


  public static void startInParentNamespace(FlatBufferBuilder builder) { builder.startObject(0); }
  public static int endInParentNamespace(FlatBufferBuilder builder) {
    int o = builder.endObject();
    return o;
  }
}

