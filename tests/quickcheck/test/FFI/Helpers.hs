{-
 Copyright 2019, Mokshasoft AB (mokshasoft.com)

 This software may be distributed and modified according to the terms of
 the GNU General Public License version 3. Note that NO WARRANTY is provided.
 See "LICENSE.txt" for details.
 -}
{-# LANGUAGE ForeignFunctionInterface #-}

{-
 FFI for the helpers part of libdill.h
 -}
module FFI.Helpers
  ( dill_now
  , dill_msleep
  ) where

import Foreign.C

foreign import ccall "dill_now" dill_now :: IO CLong

foreign import ccall "dill_msleep" dill_msleep :: CLong -> IO CInt
