/*
 * Copyright (c) 1998-2000 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#ifdef HAVE_CVS_IDENT
#ident "$Id: cprop.cc,v 1.40 2003/01/27 05:09:17 steve Exp $"
#endif

# include "config.h"

# include  "netlist.h"
# include  "netmisc.h"
# include  "functor.h"
# include  <assert.h>



/*
 * The cprop function below invokes constant propagation where
 * possible. The elaboration generates NetConst objects. I can remove
 * these and replace the gates connected to it with simpler ones. I
 * may even be able to replace nets with a new constant.
 */

struct cprop_functor  : public functor_t {

      unsigned count;

      virtual void signal(Design*des, NetNet*obj);
      virtual void lpm_add_sub(Design*des, NetAddSub*obj);
      virtual void lpm_compare(Design*des, NetCompare*obj);
      virtual void lpm_compare_eq_(Design*des, NetCompare*obj);
      virtual void lpm_ff(Design*des, NetFF*obj);
      virtual void lpm_logic(Design*des, NetLogic*obj);
      virtual void lpm_mux(Design*des, NetMux*obj);
};

void cprop_functor::signal(Design*des, NetNet*obj)
{
}

void cprop_functor::lpm_add_sub(Design*des, NetAddSub*obj)
{
	// For now, only additions are handled.
      if (obj->attribute("LPM_Direction") != verinum("ADD"))
	    return;

	// If the low bit on the A side is 0, then eliminate it from
	// the adder, and pass the B side directly to the
	// result. Don't reduce the adder smaller then a 1-bit
	// adder. These will be eliminated later.
      while ((obj->width() > 1)
	     && obj->pin_DataA(0).nexus()->drivers_constant()
	     && (obj->pin_DataA(0).nexus()->driven_value() == verinum::V0)) {

	    NetAddSub*tmp = 0;
	    tmp = new NetAddSub(obj->scope(), obj->name(), obj->width()-1);
	      //connect(tmp->pin_Aclr(), obj->pin_Aclr());
	      //connect(tmp->pin_Add_Sub(), obj->pin_Add_Sub());
	      //connect(tmp->pin_Clock(), obj->pin_Clock());
	      //connect(tmp->pin_Cin(), obj->pin_Cin());
	    connect(tmp->pin_Cout(), obj->pin_Cout());
	      //connect(tmp->pin_Overflow(), obj->pin_Overflow());
	    for (unsigned idx = 0 ;  idx < tmp->width() ;  idx += 1) {
		  connect(tmp->pin_DataA(idx), obj->pin_DataA(idx+1));
		  connect(tmp->pin_DataB(idx), obj->pin_DataB(idx+1));
		  connect(tmp->pin_Result(idx), obj->pin_Result(idx+1));
	    }
	    connect(obj->pin_Result(0), obj->pin_DataB(0));
	    delete obj;
	    des->add_node(tmp);
	    obj = tmp;
	    count += 1;
      }

	// Now do the same thing on the B side.
      while ((obj->width() > 1)
	     && obj->pin_DataB(0).nexus()->drivers_constant()
	     && (obj->pin_DataB(0).nexus()->driven_value() == verinum::V0)) {

	    NetAddSub*tmp = 0;
	    tmp = new NetAddSub(obj->scope(), obj->name(), obj->width()-1);
	      //connect(tmp->pin_Aclr(), obj->pin_Aclr());
	      //connect(tmp->pin_Add_Sub(), obj->pin_Add_Sub());
	      //connect(tmp->pin_Clock(), obj->pin_Clock());
	      //connect(tmp->pin_Cin(), obj->pin_Cin());
	    connect(tmp->pin_Cout(), obj->pin_Cout());
	      //connect(tmp->pin_Overflow(), obj->pin_Overflow());
	    for (unsigned idx = 0 ;  idx < tmp->width() ;  idx += 1) {
		  connect(tmp->pin_DataA(idx), obj->pin_DataA(idx+1));
		  connect(tmp->pin_DataB(idx), obj->pin_DataB(idx+1));
		  connect(tmp->pin_Result(idx), obj->pin_Result(idx+1));
	    }
	    connect(obj->pin_Result(0), obj->pin_DataA(0));
	    delete obj;
	    des->add_node(tmp);
	    obj = tmp;
	    count += 1;
      }

	// If the adder is only 1 bit wide, then replace it with the
	// simple logic gate.
      if (obj->width() == 1) {
	    NetLogic*tmp;
	    if (obj->pin_Cout().is_linked()) {
		  tmp = new NetLogic(obj->scope(),
				     des->local_symbol(obj->name()), 3,
				     NetLogic::AND);
		  connect(tmp->pin(0), obj->pin_Cout());
		  connect(tmp->pin(1), obj->pin_DataA(0));
		  connect(tmp->pin(2), obj->pin_DataB(0));
		  des->add_node(tmp);
	    }
	    tmp = new NetLogic(obj->scope(), obj->name(), 3, NetLogic::XOR);
	    connect(tmp->pin(0), obj->pin_Result(0));
	    connect(tmp->pin(1), obj->pin_DataA(0));
	    connect(tmp->pin(2), obj->pin_DataB(0));
	    delete obj;
	    des->add_node(tmp);
	    count += 1;
	    return;
      }

}

void cprop_functor::lpm_compare(Design*des, NetCompare*obj)
{
      if (obj->pin_AEB().is_linked()) {
	    assert( ! obj->pin_AGB().is_linked() );
	    assert( ! obj->pin_AGEB().is_linked() );
	    assert( ! obj->pin_ALB().is_linked() );
	    assert( ! obj->pin_ALEB().is_linked() );
	    assert( ! obj->pin_AGB().is_linked() );
	    assert( ! obj->pin_ANEB().is_linked() );
	    lpm_compare_eq_(des, obj);
	    return;
      }
}

void cprop_functor::lpm_compare_eq_(Design*des, NetCompare*obj)
{
      NetScope*scope = obj->scope();

	/* First, look for the case where constant bits on matching A
	   and B inputs are different. This this is so, the device can
	   be completely eliminated and replaced with a constant 0. */

      for (unsigned idx = 0 ;  idx < obj->width() ;  idx += 1) {
	    if (! obj->pin_DataA(idx).nexus()->drivers_constant())
		  continue;
	    if (! obj->pin_DataB(idx).nexus()->drivers_constant())
		  continue;
	    if (obj->pin_DataA(idx).nexus()->driven_value() ==
		obj->pin_DataB(idx).nexus()->driven_value())
		  continue;

	    NetConst*zero = new NetConst(scope, obj->name(), verinum::V0);
	    connect(zero->pin(0), obj->pin_AEB());
	    delete obj;
	    des->add_node(zero);
	    count += 1;
	    return;
      }

	/* Still may need the gate. Run through the inputs again, and
	   look for pairs of constants. Those inputs can be removed. */

      unsigned top = obj->width();
      for (unsigned idx = 0 ;  idx < top ; ) {
	    if (! obj->pin_DataA(idx).nexus()->drivers_constant()) {
		  idx += 1;
		  continue;
	    }
	    if (! obj->pin_DataB(idx).nexus()->drivers_constant()) {
		  idx += 1;
		  continue;
	    }

	    obj->pin_DataA(idx).unlink();
	    obj->pin_DataB(idx).unlink();

	    top -= 1;
	    for (unsigned jj = idx ;  jj < top ;  jj += 1) {
		  connect(obj->pin_DataA(jj), obj->pin_DataA(jj+1));
		  connect(obj->pin_DataB(jj), obj->pin_DataB(jj+1));
		  obj->pin_DataA(jj+1).unlink();
		  obj->pin_DataB(jj+1).unlink();
	    }
      }

	/* If we wound up disconnecting all the inputs, then remove
	   the device and replace it with a constant. */
      if (top == 0) {
	    NetConst*one = new NetConst(scope, obj->name(), verinum::V1);
	    connect(one->pin(0), obj->pin_AEB());
	    delete obj;
	    des->add_node(one);
	    count += 1;
	    return;
      }

	/* If there is only one bit left, then replace the comparator
	   with a simple XOR gate. */
      if (top == 1) {
	    NetLogic*tmp = new NetLogic(scope, obj->name(), 3,
					NetLogic::XNOR);
	    connect(tmp->pin(0), obj->pin_AEB());
	    connect(tmp->pin(1), obj->pin_DataA(0));
	    connect(tmp->pin(2), obj->pin_DataB(0));
	    delete obj;
	    des->add_node(tmp);
	    count += 1;
	    return;
      }


      if (top == obj->width())
	    return;

      NetCompare*tmp = new NetCompare(scope, obj->name(), top);
      connect(tmp->pin_AEB(), obj->pin_AEB());
      for (unsigned idx = 0 ;  idx < top ;  idx += 1) {
	    connect(tmp->pin_DataA(idx), obj->pin_DataA(idx));
	    connect(tmp->pin_DataB(idx), obj->pin_DataB(idx));
      }
      delete obj;
      des->add_node(tmp);
      count += 1;
}

void cprop_functor::lpm_ff(Design*des, NetFF*obj)
{
	// Look for and count unlinked FF outputs. Note that if the
	// Data and Q pins are connected together, they can be removed
	// from the circuit.
      unsigned unlinked_count = 0;
      for (unsigned idx = 0 ;  idx < obj->width() ;  idx += 1) {
	    if (connected(obj->pin_Data(idx), obj->pin_Q(idx))) {
		  obj->pin_Data(idx).unlink();
		  obj->pin_Q(idx).unlink();
	    }
	    if (! obj->pin_Q(idx).is_linked())
		  unlinked_count += 1;
      }

	// If the entire FF is unlinked, remove the whole thing.
      if (unlinked_count == obj->width()) {
	    delete obj;
	    count += 1;
	    return;
      }

	// If some of the FFs are unconnected, make a new FF array
	// that does not include the useless FF devices.
      if (unlinked_count > 0) {
	    NetFF*tmp = new NetFF(obj->scope(), obj->name(),
				  obj->width()-unlinked_count);
	    connect(tmp->pin_Clock(), obj->pin_Clock());
	    connect(tmp->pin_Enable(), obj->pin_Enable());
	    connect(tmp->pin_Aload(), obj->pin_Aload());
	    connect(tmp->pin_Aset(), obj->pin_Aset());
	    connect(tmp->pin_Aclr(), obj->pin_Aclr());
	    connect(tmp->pin_Sload(), obj->pin_Sload());
	    connect(tmp->pin_Sset(), obj->pin_Sset());
	    connect(tmp->pin_Sclr(), obj->pin_Sclr());

	    unsigned tidx = 0;
	    for (unsigned idx = 0 ;  idx < obj->width() ;  idx += 1)
		  if (obj->pin_Q(idx).is_linked()) {
			connect(tmp->pin_Data(tidx), obj->pin_Data(idx));
			connect(tmp->pin_Q(tidx), obj->pin_Q(idx));
			tidx += 1;
		  }

	    assert(tidx == obj->width() - unlinked_count);
	    delete obj;
	    des->add_node(tmp);
	    count += 1;
	    return;
      }
}

void cprop_functor::lpm_logic(Design*des, NetLogic*obj)
{
      NetScope*scope = obj->scope();

      switch (obj->type()) {

	  case NetLogic::NAND:
	  case NetLogic::AND: {
		unsigned top = obj->pin_count();
		unsigned idx = 1;
		unsigned xs = 0;

		  /* Eliminate all the 1 inputs. They have no effect
		     on the output of an AND gate. */

		while (idx < top) {
		      if (! obj->pin(idx).nexus()->drivers_constant()) {
			    idx += 1;
			    continue;
		      }

		      if (obj->pin(idx).nexus()->driven_value()==verinum::V1) {
			    obj->pin(idx).unlink();
			    top -= 1;
			    if (idx < top) {
				  connect(obj->pin(idx), obj->pin(top));
				  obj->pin(top).unlink();
			    }

			    continue;
		      }

		      if (obj->pin(idx).nexus()->driven_value() != verinum::V0) {
			    idx += 1;
			    xs += 1;
			    continue;
		      }

			/* Oops! We just stumbled on a driven-0 input
			   to the AND gate. That means we can replace
			   the whole bloody thing with a constant
			   driver and exit now. */
		      NetConst*tmp;
		      switch (obj->type()) {
			  case NetLogic::AND:
			    tmp = new NetConst(scope, obj->name(), verinum::V0);
			    break;
			  case NetLogic::NAND:
			    tmp = new NetConst(scope, obj->name(), verinum::V1);
			    break;
			  default:
			    assert(0);
		      }

		      tmp->rise_time(obj->rise_time());
		      tmp->fall_time(obj->fall_time());
		      tmp->decay_time(obj->decay_time());

		      des->add_node(tmp);
		      tmp->pin(0).drive0(obj->pin(0).drive0());
		      tmp->pin(0).drive1(obj->pin(0).drive1());
		      connect(obj->pin(0), tmp->pin(0));

		      delete obj;
		      count += 1;
		      return;
		}

		  /* If all the inputs were eliminated, then replace
		     the gate with a constant 1 and I am done. */
		if (top == 1) {
		      NetConst*tmp;
		      switch (obj->type()) {
			  case NetLogic::AND:
			    tmp = new NetConst(scope, obj->name(), verinum::V1);
			    break;
			  case NetLogic::NAND:
			    tmp = new NetConst(scope, obj->name(), verinum::V0);
			    break;
			  default:
			    assert(0);
		      }

		      tmp->rise_time(obj->rise_time());
		      tmp->fall_time(obj->fall_time());
		      tmp->decay_time(obj->decay_time());

		      des->add_node(tmp);
		      tmp->pin(0).drive0(obj->pin(0).drive0());
		      tmp->pin(0).drive1(obj->pin(0).drive1());
		      connect(obj->pin(0), tmp->pin(0));

		      delete obj;
		      count += 1;
		      return;
		}

		  /* If all the inputs are unknowns, then replace the
		     gate with a Vx. */
		if (xs == (top-1)) {
		      NetConst*tmp;
		      tmp = new NetConst(scope, obj->name(), verinum::Vx);
		      des->add_node(tmp);
		      tmp->pin(0).drive0(obj->pin(0).drive0());
		      tmp->pin(0).drive1(obj->pin(0).drive1());
		      connect(obj->pin(0), tmp->pin(0));

		      delete obj;
		      count += 1;
		      return;
		}

		  /* If we are down to only one input, then replace
		     the AND with a BUF and exit now. */
		if (top == 2) {
		      NetLogic*tmp;
		      switch (obj->type()) {
			  case NetLogic::AND:
			    tmp = new NetLogic(scope,
					       obj->name(), 2,
					       NetLogic::BUF);
			    break;
			  case NetLogic::NAND:
			    tmp = new NetLogic(scope,
					       obj->name(), 2,
					       NetLogic::NOT);
			    break;
			  default:
			    assert(0);
		      }

		      tmp->rise_time(obj->rise_time());
		      tmp->fall_time(obj->fall_time());
		      tmp->decay_time(obj->decay_time());

		      des->add_node(tmp);
		      tmp->pin(0).drive0(obj->pin(0).drive0());
		      tmp->pin(0).drive1(obj->pin(0).drive1());
		      connect(obj->pin(0), tmp->pin(0));
		      connect(obj->pin(1), tmp->pin(1));
		      delete obj;
		      count += 1;
		      return;
		}

		  /* Finally, this cleans up the gate by creating a
		     new [N]AND gate that has the right number of
		     inputs, connected in the right place. */
		if (top < obj->pin_count()) {
		      NetLogic*tmp = new NetLogic(scope,
						  obj->name(), top,
						  obj->type());
		      tmp->rise_time(obj->rise_time());
		      tmp->fall_time(obj->fall_time());
		      tmp->decay_time(obj->decay_time());

		      des->add_node(tmp);
		      tmp->pin(0).drive0(obj->pin(0).drive0());
		      tmp->pin(0).drive1(obj->pin(0).drive1());
		      for (unsigned idx = 0 ;  idx < top ;  idx += 1)
			    connect(tmp->pin(idx), obj->pin(idx));

		      delete obj;
		      count += 1;
		      return;
		}
		break;
	  }


	  case NetLogic::NOR:
	  case NetLogic::OR: {
		unsigned top = obj->pin_count();
		unsigned idx = 1;


		  /* Eliminate all the 0 inputs. They have no effect
		     on the output of an OR gate. */

		while (idx < top) {
		      if (! obj->pin(idx).nexus()->drivers_constant()) {
			    idx += 1;
			    continue;
		      }

		      if (obj->pin(idx).nexus()->driven_value() == verinum::V0) {
			    obj->pin(idx).unlink();
			    top -= 1;
			    if (idx < top) {
				  connect(obj->pin(idx), obj->pin(top));
				  obj->pin(top).unlink();
			    }

			    continue;
		      }

		      if (obj->pin(idx).nexus()->driven_value() != verinum::V1) {
			    idx += 1;
			    continue;
		      }

			/* Oops! We just stumbled on a driven-1 input
			   to the OR gate. That means we can replace
			   the whole bloody thing with a constant
			   driver and exit now. */
		      NetConst*tmp;
		      switch (obj->type()) {
			  case NetLogic::OR:
			    tmp = new NetConst(scope, obj->name(), verinum::V1);
			    break;
			  case NetLogic::NOR:
			    tmp = new NetConst(scope, obj->name(), verinum::V0);
			    break;
			  default:
			    assert(0);
		      }

		      tmp->rise_time(obj->rise_time());
		      tmp->fall_time(obj->fall_time());
		      tmp->decay_time(obj->decay_time());

		      des->add_node(tmp);
		      tmp->pin(0).drive0(obj->pin(0).drive0());
		      tmp->pin(0).drive1(obj->pin(0).drive1());
		      connect(obj->pin(0), tmp->pin(0));

		      delete obj;
		      count += 1;
		      return;
		}

		  /* If all the inputs were eliminated, then replace
		     the gate with a constant 0 and I am done. */
		if (top == 1) {
		      NetConst*tmp;
		      switch (obj->type()) {
			  case NetLogic::OR:
			    tmp = new NetConst(scope, obj->name(), verinum::V0);
			    break;
			  case NetLogic::NOR:
			    tmp = new NetConst(scope, obj->name(), verinum::V1);
			    break;
			  default:
			    assert(0);
		      }

		      tmp->rise_time(obj->rise_time());
		      tmp->fall_time(obj->fall_time());
		      tmp->decay_time(obj->decay_time());

		      des->add_node(tmp);
		      tmp->pin(0).drive0(obj->pin(0).drive0());
		      tmp->pin(0).drive1(obj->pin(0).drive1());
		      connect(obj->pin(0), tmp->pin(0));

		      delete obj;
		      count += 1;
		      return;
		}

		  /* If we are down to only one input, then replace
		     the OR with a BUF and exit now. */
		if (top == 2) {
		      NetLogic*tmp;
		      switch (obj->type()) {
			  case NetLogic::OR:
			    tmp = new NetLogic(scope,
					       obj->name(), 2,
					       NetLogic::BUF);
			    break;
			  case NetLogic::NOR:
			    tmp = new NetLogic(scope,
					       obj->name(), 2,
					       NetLogic::NOT);
			    break;
			  default:
			    assert(0);
		      }
		      tmp->rise_time(obj->rise_time());
		      tmp->fall_time(obj->fall_time());
		      tmp->decay_time(obj->decay_time());

		      des->add_node(tmp);
		      tmp->pin(0).drive0(obj->pin(0).drive0());
		      tmp->pin(0).drive1(obj->pin(0).drive1());
		      connect(obj->pin(0), tmp->pin(0));
		      connect(obj->pin(1), tmp->pin(1));
		      delete obj;
		      count += 1;
		      return;
		}

		  /* Finally, this cleans up the gate by creating a
		     new [N]OR gate that has the right number of
		     inputs, connected in the right place. */
		if (top < obj->pin_count()) {
		      NetLogic*tmp = new NetLogic(scope,
						  obj->name(), top,
						  obj->type());
		      tmp->rise_time(obj->rise_time());
		      tmp->fall_time(obj->fall_time());
		      tmp->decay_time(obj->decay_time());

		      des->add_node(tmp);
		      tmp->pin(0).drive0(obj->pin(0).drive0());
		      tmp->pin(0).drive1(obj->pin(0).drive1());
		      for (unsigned idx = 0 ;  idx < top ;  idx += 1)
			    connect(tmp->pin(idx), obj->pin(idx));

		      delete obj;
		      count += 1;
		      return;
		}
		break;
	  }

	  case NetLogic::XNOR:
	  case NetLogic::XOR: {
		unsigned top = obj->pin_count();
		unsigned idx = 1;

		  /* Eliminate all the 0 inputs. They have no effect
		     on the output of an XOR gate. The eliminate works
		     by unlinking the current input and relinking the
		     last input to this position. It's like bubbling
		     all the 0 inputs to the end. */
		while (idx < top) {
		      if (! obj->pin(idx).nexus()->drivers_constant()) {
			    idx += 1;
			    continue;
		      }

		      if (obj->pin(idx).nexus()->driven_value() == verinum::V0) {
			    obj->pin(idx).unlink();
			    top -= 1;
			    if (idx < top) {
				  connect(obj->pin(idx), obj->pin(top));
				  obj->pin(top).unlink();
			    }

		      } else {
			    idx += 1;
		      }
		}

		  /* Look for pairs of constant 1 inputs. If I find a
		     pair, then eliminate both. Each iteration through
		     the loop, the `one' variable holds the index to
		     the previous V1, or 0 if there is none.

		     The `ones' variable counts the number of V1
		     inputs. After this loop completes, `ones' will be
		     0 or 1. */

		unsigned one = 0, ones = 0;
		idx = 1;
		while (idx < top) {
		      if (! obj->pin(idx).nexus()->drivers_constant()) {
			    idx += 1;
			    continue;
		      }

		      if (obj->pin(idx).nexus()->driven_value() == verinum::V1) {
			    if (one == 0) {
				  one = idx;
				  ones += 1;
				  idx += 1;
				  continue;
			    }

			      /* Here we found two constant V1
				 inputs. Unlink both. */
			    obj->pin(idx).unlink();
			    top -= 1;
			    if (idx < top) {
				  connect(obj->pin(idx), obj->pin(top));
				  obj->pin(top).unlink();
			    }

			    obj->pin(one).unlink();
			    top -= 1;
			    if (one < top) {
				  connect(obj->pin(one), obj->pin(top));
				  obj->pin(top).unlink();
			    }

			      /* Reset ones counter and one index,
				 start looking for the next pair. */
			    assert(ones == 1);
			    ones = 0;
			    one  = 0;
			    continue;
		      }

		      idx += 1;
		}

		  /* If all the inputs were eliminated, then replace
		     the gate with a constant value and I am done. */
		if (top == 1) {
		      verinum::V out = obj->type()==NetLogic::XNOR
			    ? verinum::V1
			    : verinum::V0;
		      NetConst*tmp = new NetConst(scope, obj->name(), out);

		      tmp->rise_time(obj->rise_time());
		      tmp->fall_time(obj->fall_time());
		      tmp->decay_time(obj->decay_time());

		      des->add_node(tmp);
		      tmp->pin(0).drive0(obj->pin(0).drive0());
		      tmp->pin(0).drive1(obj->pin(0).drive1());
		      connect(obj->pin(0), tmp->pin(0));

		      delete obj;
		      count += 1;
		      return;
		}

		  /* If there is a stray V1 input and only one other
		     input, then replace the gate with an inverter and
		     we are done. */

		if ((top == 3) && (ones == 1)) {
		      unsigned save;
		      if (! obj->pin(1).nexus()->drivers_constant())
			    save = 1;
		      else if (obj->pin(1).nexus()->driven_value() != verinum::V1)
			    save = 1;
		      else
			    save = 2;

		      NetLogic*tmp;

		      if (obj->type() == NetLogic::XOR)
			    tmp = new NetLogic(scope,
					       obj->name(), 2,
					       NetLogic::NOT);
		      else
			    tmp = new NetLogic(scope,
					       obj->name(), 2,
					       NetLogic::BUF);

		      tmp->rise_time(obj->rise_time());
		      tmp->fall_time(obj->fall_time());
		      tmp->decay_time(obj->decay_time());

		      des->add_node(tmp);
		      tmp->pin(0).drive0(obj->pin(0).drive0());
		      tmp->pin(0).drive1(obj->pin(0).drive1());
		      connect(obj->pin(0), tmp->pin(0));
		      connect(obj->pin(save), tmp->pin(1));

		      delete obj;
		      count += 1;
		      return;
		}

		  /* If we are down to only one input, then replace
		     the XOR with a BUF and exit now. */
		if (top == 2) {
		      NetLogic*tmp;

		      if (obj->type() == NetLogic::XOR)
			    tmp = new NetLogic(scope,
					       obj->name(), 2,
					       NetLogic::BUF);
		      else
			    tmp = new NetLogic(scope,
					       obj->name(), 2,
					       NetLogic::NOT);

		      tmp->rise_time(obj->rise_time());
		      tmp->fall_time(obj->fall_time());
		      tmp->decay_time(obj->decay_time());

		      des->add_node(tmp);
		      tmp->pin(0).drive0(obj->pin(0).drive0());
		      tmp->pin(0).drive1(obj->pin(0).drive1());
		      connect(obj->pin(0), tmp->pin(0));
		      connect(obj->pin(1), tmp->pin(1));
		      delete obj;
		      count += 1;
		      return;
		}

		  /* Finally, this cleans up the gate by creating a
		     new XOR gate that has the right number of
		     inputs, connected in the right place. */
		if (top < obj->pin_count()) {
		      NetLogic*tmp = new NetLogic(scope,
						  obj->name(), top,
						  obj->type());
		      des->add_node(tmp);
		      tmp->pin(0).drive0(obj->pin(0).drive0());
		      tmp->pin(0).drive1(obj->pin(0).drive1());
		      for (unsigned idx = 0 ;  idx < top ;  idx += 1)
			    connect(tmp->pin(idx), obj->pin(idx));

		      delete obj;
		      count += 1;
		      return;
		}
		break;
	    }

	  default:
	    break;
      }
}

/*
 * This detects the case where the mux selects between a value an
 * Vz. In this case, replace the device with a bufif with the sel
 * input used to enable the output.
 */
void cprop_functor::lpm_mux(Design*des, NetMux*obj)
{
      if (obj->size() != 2)
	    return;
      if (obj->sel_width() != 1)
	    return;

	/* If the first input is all constant Vz, then replace the
	   NetMux with an array of BUFIF1 devices, with the enable
	   connected to the select input. */
      bool flag = true;
      for (unsigned idx = 0 ;  idx < obj->width() ;  idx += 1) {
	    if (! obj->pin_Data(idx, 0).nexus()->drivers_constant()) {
		  flag = false;
		  break;
	    }

	    if (obj->pin_Data(idx, 0).nexus()->driven_value() != verinum::Vz) {
		  flag = false;
		  break;
	    }
      }

      if (flag) {
	    for (unsigned idx = 0 ;  idx < obj->width() ;  idx += 1) {
		  NetLogic*tmp = new NetLogic(obj->scope(),
					      des->local_symbol(obj->name()),
					      3, NetLogic::BUFIF1);

		  connect(obj->pin_Result(idx), tmp->pin(0));
		  connect(obj->pin_Data(idx,1), tmp->pin(1));
		  connect(obj->pin_Sel(0), tmp->pin(2));
		  des->add_node(tmp);
	    }

	    count += 1;
	    delete obj;
	    return;
      }

	/* If instead the second input is all constant Vz, replace the
	   NetMux with an array of BUFIF0 devices. */
      flag = true;
      for (unsigned idx = 0 ;  idx < obj->width() ;  idx += 1) {
	    if (! obj->pin_Data(idx, 1).nexus()->drivers_constant()) {
		  flag = false;
		  break;
	    }

	    if (obj->pin_Data(idx, 1).nexus()->driven_value() != verinum::Vz) {
		  flag = false;
		  break;
	    }
      }

      if (flag) {
	    for (unsigned idx = 0 ;  idx < obj->width() ;  idx += 1) {
		  NetLogic*tmp = new NetLogic(obj->scope(),
					      des->local_symbol(obj->name()),
					      3, NetLogic::BUFIF0);

		  connect(obj->pin_Result(idx), tmp->pin(0));
		  connect(obj->pin_Data(idx,0), tmp->pin(1));
		  connect(obj->pin_Sel(0), tmp->pin(2));
		  des->add_node(tmp);
	    }

	    count += 1;
	    delete obj;
	    return;
      }
}

/*
 * This functor looks to see if the constant is connected to nothing
 * but signals. If that is the case, delete the dangling constant and
 * the now useless signals. This functor is applied after the regular
 * functor to clean up dangling constants that might be left behind.
 */
struct cprop_dc_functor  : public functor_t {

      virtual void lpm_const(Design*des, NetConst*obj);
};

void cprop_dc_functor::lpm_const(Design*des, NetConst*obj)
{
	// 'bz constant values drive high impedance to whatever is
	// connected to it. In other words, it is a noop.
      { unsigned tmp = 0;
        for (unsigned idx = 0 ;  idx < obj->pin_count() ;  idx += 1)
	      if (obj->value(idx) == verinum::Vz) {
		    obj->pin(idx).unlink();
		    tmp += 1;
	      }

	if (tmp == obj->pin_count()) {
	      delete obj;
	      return;
	}
      }

	// For each bit, if this is the only driver, then set the
	// initial value of all the signals to this value.
      for (unsigned idx = 0 ;  idx < obj->pin_count() ;  idx += 1) {
	    if (count_outputs(obj->pin(idx)) > 1)
		  continue;

	    Nexus*nex = obj->pin(idx).nexus();
	    for (Link*clnk = nex->first_nlink()
		       ; clnk ; clnk = clnk->next_nlink()) {

		  NetObj*cur;
		  unsigned pin;
		  clnk->cur_link(cur, pin);

		  NetNet*tmp = dynamic_cast<NetNet*>(cur);
		  if (tmp == 0)
			continue;

		  tmp->pin(pin).set_init(obj->value(idx));
	    }
      }

	// If there are any links that take input, the constant is
	// used structurally somewhere.
      for (unsigned idx = 0 ;  idx < obj->pin_count() ;  idx += 1)
	    if (count_inputs(obj->pin(idx)) > 0)
		  return;

	// Look for signals that have NetESignal nodes attached to
	// them. If I find any, then this constant is used by a
	// behavioral expression somewhere.
      for (unsigned idx = 0 ;  idx < obj->pin_count() ;  idx += 1) {
	    Nexus*nex = obj->pin(idx).nexus();
	    for (Link*clnk = nex->first_nlink()
		       ; clnk ; clnk = clnk->next_nlink()) {

		  NetObj*cur;
		  unsigned pin;
		  clnk->cur_link(cur, pin);

		  NetNet*tmp = dynamic_cast<NetNet*>(cur);
		  if (tmp == 0)
			continue;

		  assert(tmp->scope());

		    // If the net has an eref, then there is an
		    // expression somewhere that reads this signal. So
		    // the constant does get read.
		  if (tmp->peek_eref() > 0)
			return;

		    // If the net is a port of the root module, then
		    // the constant may be driving something outside
		    // the design, so do not eliminate it.
		  if ((tmp->port_type() != NetNet::NOT_A_PORT)
		      && (tmp->scope()->parent() == 0))
			return;

	    }
      }

	// Done. Delete me.
      delete obj;
}


void cprop(Design*des)
{
	// Continually propogate constants until a scan finds nothing
	// to do.
      cprop_functor prop;
      do {
	    prop.count = 0;
	    des->functor(&prop);
      } while (prop.count > 0);

      cprop_dc_functor dc;
      des->functor(&dc);
}

/*
 * $Log: cprop.cc,v $
 * Revision 1.40  2003/01/27 05:09:17  steve
 *  Spelling fixes.
 *
 * Revision 1.39  2002/08/20 04:12:22  steve
 *  Copy gate delays when doing gate delay substitutions.
 *
 * Revision 1.38  2002/08/12 01:34:58  steve
 *  conditional ident string using autoconfig.
 *
 * Revision 1.37  2002/06/25 01:33:22  steve
 *  Cache calculated driven value.
 *
 * Revision 1.36  2002/06/24 01:49:38  steve
 *  Make link_drive_constant cache its results in
 *  the Nexus, to improve cprop performance.
 *
 * Revision 1.35  2002/05/26 01:39:02  steve
 *  Carry Verilog 2001 attributes with processes,
 *  all the way through to the ivl_target API.
 *
 *  Divide signal reference counts between rval
 *  and lval references.
 *
 * Revision 1.34  2002/05/23 03:08:51  steve
 *  Add language support for Verilog-2001 attribute
 *  syntax. Hook this support into existing $attribute
 *  handling, and add number and void value types.
 *
 *  Add to the ivl_target API new functions for access
 *  of complex attributes attached to gates.
 *
 * Revision 1.33  2002/04/14 02:51:37  steve
 *  Fix bug removing pairs of ones in XOR.
 *
 * Revision 1.32  2002/02/03 00:06:28  steve
 *  Comments about xor evaluation.
 *
 * Revision 1.31  2001/12/31 01:56:08  steve
 *  Get sense of 1-bit == operator right.
 *
 * Revision 1.30  2001/10/28 01:14:53  steve
 *  NetObj constructor finally requires a scope.
 *
 * Revision 1.29  2001/07/25 03:10:48  steve
 *  Create a config.h.in file to hold all the config
 *  junk, and support gcc 3.0. (Stephan Boettcher)
 *
 * Revision 1.28  2001/06/15 04:14:18  steve
 *  Generate vvp code for GT and GE comparisons.
 *
 * Revision 1.27  2001/06/07 02:12:43  steve
 *  Support structural addition.
 *
 * Revision 1.26  2001/02/18 01:07:32  steve
 *  check signals in the cprop functor.
 *
 * Revision 1.25  2001/02/16 03:27:31  steve
 *  Constant propagation for compare ==.
 *
 * Revision 1.24  2001/02/10 04:50:54  steve
 *  Catch constants driving root module ports. (PR#130)
 *
 * Revision 1.23  2000/12/30 03:11:15  steve
 *  Propagate initial value of constants into wires.
 *
 * Revision 1.22  2000/11/23 01:55:52  steve
 *  Propagate constants through xnor gates. (PR#51)
 *
 * Revision 1.21  2000/11/19 05:26:58  steve
 *  Replace AND constand propagation.
 *
 * Revision 1.20  2000/11/18 05:13:27  steve
 *  Thorough constant propagation for or and nor gates.
 *
 * Revision 1.19  2000/11/18 04:10:37  steve
 *  Handle constant propagation through XOR gates,
 *  including reducing the gate to a constant,
 *  a buffer or an inverter if possible.
 *
 * Revision 1.18  2000/11/11 00:03:36  steve
 *  Add support for the t-dll backend grabing flip-flops.
 *
 * Revision 1.17  2000/10/07 19:45:42  steve
 *  Put logic devices into scopes.
 *
 * Revision 1.16  2000/10/06 21:26:34  steve
 *  Eliminate zero inputs to xor.
 *
 * Revision 1.15  2000/08/02 14:48:01  steve
 *  use bufif0 if z is in true case of mux.
 *
 * Revision 1.14  2000/07/25 02:55:13  steve
 *  Unlink z constants from nets.
 *
 * Revision 1.13  2000/07/15 05:13:43  steve
 *  Detect muxing Vz as a bufufN.
 *
 * Revision 1.12  2000/06/25 19:59:41  steve
 *  Redesign Links to include the Nexus class that
 *  carries properties of the connected set of links.
 *
 * Revision 1.11  2000/06/24 22:55:19  steve
 *  Get rid of useless next_link method.
 *
 * Revision 1.10  2000/05/14 17:55:04  steve
 *  Support initialization of FF Q value.
 *
 * Revision 1.9  2000/05/07 04:37:56  steve
 *  Carry strength values from Verilog source to the
 *  pform and netlist for gates.
 *
 *  Change vvm constants to use the driver_t to drive
 *  a constant value. This works better if there are
 *  multiple drivers on a signal.
 *
 * Revision 1.8  2000/04/28 21:00:28  steve
 *  Over agressive signal elimination in constant probadation.
 *
 * Revision 1.7  2000/02/23 02:56:54  steve
 *  Macintosh compilers do not support ident.
 *
 * Revision 1.6  2000/01/02 17:56:42  steve
 *  Do not delete constants that input to exressions.
 *
 * Revision 1.5  1999/12/30 04:19:12  steve
 *  Propogate constant 0 in low bits of adders.
 *
 * Revision 1.4  1999/12/17 06:18:15  steve
 *  Rewrite the cprop functor to use the functor_t interface.
 *
 * Revision 1.3  1999/12/17 03:38:46  steve
 *  NetConst can now hold wide constants.
 *
 * Revision 1.2  1998/12/02 04:37:13  steve
 *  Add the nobufz function to eliminate bufz objects,
 *  Object links are marked with direction,
 *  constant propagation is more careful will wide links,
 *  Signal folding is aware of attributes, and
 *  the XNF target can dump UDP objects based on LCA
 *  attributes.
 *
 * Revision 1.1  1998/11/13 06:23:17  steve
 *  Introduce netlist optimizations with the
 *  cprop function to do constant propogation.
 *
 */

