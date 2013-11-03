﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MLVViewSharp
{
    public class Matrix
    {
		private decimal[,] mInnerMatrix;
		private int mRowCount = 0;
		public int RowCount
		{
			get { return mRowCount; }
		}
		private int mColumnCount = 0;
		public int ColumnCount
		{
			get { return mColumnCount; }
		}

		public Matrix()
		{

        }

        public Matrix(Matrix source)
        {
            mRowCount = source.RowCount;
            mColumnCount = source.ColumnCount;
            mInnerMatrix = new decimal[mRowCount, mColumnCount];

            for (int row = 0; row < mRowCount; row++)
            {
                for (int col = 0; col < mColumnCount; col++)
                {
                    mInnerMatrix[row, col] = source.mInnerMatrix[row, col];
                }
            }
        }

        public Matrix(int rowCount, int columnCount)
        {
            mRowCount = rowCount;
            mColumnCount = columnCount;
            mInnerMatrix = new decimal[rowCount, columnCount];
        }

		public decimal this[int rowNumber,int columnNumber]
		{
			get
			{
				return mInnerMatrix[rowNumber, columnNumber];
			}
			set
			{
				mInnerMatrix[rowNumber, columnNumber] = value;
			}
		}

		public decimal[] GetRow(int rowIndex)
		{
			decimal[] rowValues = new decimal[mColumnCount];
			for (int i = 0; i < mColumnCount; i++)
			{
				rowValues[i] = mInnerMatrix[rowIndex, i];
			}
			return rowValues;
		}
		public void SetRow(int rowIndex,decimal[] value)
		{
			if (value.Length != mColumnCount)
			{
				throw new Exception("Boyut Uyusmazligi");
			}
			for (int i = 0; i < value.Length; i++)
			{
				mInnerMatrix[rowIndex, i] = value[i];
			}
		}
		public decimal[] GetColumn(int columnIndex)
		{ 
			decimal[] columnValues = new decimal[mRowCount];
			for (int i = 0; i < mRowCount; i++)
			{
				columnValues[i] = mInnerMatrix[i, columnIndex];
			}
			return columnValues;
		}
		public void SetColumn(int columnIndex,decimal[] value)
		{ 
			if (value.Length != mRowCount)
			{
				throw new Exception("Boyut Uyusmazligi");
			}
			for (int i = 0; i < value.Length; i++)
			{
				mInnerMatrix[i, columnIndex] = value[i];
			}
		}

		
		public static Matrix operator +(Matrix pMatrix1,Matrix pMatrix2)
		{
			if (!(pMatrix1.RowCount == pMatrix2.RowCount && pMatrix1.ColumnCount == pMatrix2.ColumnCount))
			{
				throw new Exception("Boyut Uyusmazligi");
			}
			Matrix returnMartix = new Matrix(pMatrix1.RowCount, pMatrix2.RowCount);
			for (int i = 0; i < pMatrix1.RowCount; i++)
			{
				for (int j = 0; j < pMatrix1.ColumnCount; j++)
				{
					returnMartix[i, j] = pMatrix1[i, j] + pMatrix2[i, j];
				}
			}
			return returnMartix;
		}
		public static Matrix operator *(decimal scalarValue, Matrix pMatrix)
		{
			Matrix returnMartix = new Matrix(pMatrix.RowCount, pMatrix.RowCount);
			for (int i = 0; i < pMatrix.RowCount; i++)
			{
				for (int j = 0; j < pMatrix.ColumnCount; j++)
				{
					returnMartix[i, j] = pMatrix[i, j] * scalarValue;
				}
			}
			return returnMartix;
		}
		public static Matrix operator -(Matrix pMatrix1, Matrix pMatrix2)
		{
			if (!(pMatrix1.RowCount == pMatrix2.RowCount && pMatrix1.ColumnCount == pMatrix2.ColumnCount))
			{
				throw new Exception("Boyut Uyusmazligi");
			}
			return pMatrix1 + (-1 * pMatrix2);
		}
		public static bool operator ==(Matrix pMatrix1, Matrix pMatrix2)
		{
			if (!(pMatrix1.RowCount == pMatrix2.RowCount && pMatrix1.ColumnCount == pMatrix2.ColumnCount))
			{
				//boyut uyusmazligi
				return false;
			}
			for (int i = 0; i < pMatrix1.RowCount; i++)
			{
				for (int j = 0; j < pMatrix1.ColumnCount; j++)
				{
					if (pMatrix1[i, j] != pMatrix2[i, j])
					{
						return false;
					}
				}
			}
			return true; ;
		}
		public static bool operator !=(Matrix pMatrix1, Matrix pMatrix2)
		{
			return !(pMatrix1 == pMatrix2);
		}
		public static Matrix operator -(Matrix pMatrix)
		{
			return -1 * pMatrix;
		}
		public static Matrix operator ++(Matrix pMatrix)
		{
			
			for (int i = 0; i < pMatrix.RowCount; i++)
			{
				for (int j = 0; j < pMatrix.ColumnCount; j++)
				{
					pMatrix[i, j] += 1;
				}
			}
			return pMatrix ;
		}
		public static Matrix operator --(Matrix pMatrix)
		{
			for (int i = 0; i < pMatrix.RowCount; i++)
			{
				for (int j = 0; j < pMatrix.ColumnCount; j++)
				{
					pMatrix[i, j] -= 1;
				}
			}
			return pMatrix;
		}
		public static Matrix operator *(Matrix pMatrix1, Matrix pMatrix2)
		{
			if (pMatrix1.ColumnCount != pMatrix2.RowCount)
			{
				throw new Exception("Boyut Uyusmazligi");
			}
			Matrix returnMatrix = new Matrix(pMatrix1.RowCount, pMatrix2.ColumnCount);
			for (int i = 0; i<pMatrix1.RowCount; i++)
			{
				decimal[] rowValues = pMatrix1.GetRow(i);
				for (int j = 0; j<pMatrix2.ColumnCount; j++)
				{
					decimal[] columnValues = pMatrix2.GetColumn(j);
					decimal value=0;
					for (int a = 0; a < rowValues.Length; a++)
					{
						value+=rowValues[a] * columnValues[a];
					}
					returnMatrix[i, j] = value;
				}
			}
			return returnMatrix;
        }
		public Matrix Transpose()
		{
			Matrix mReturnMartix = new Matrix(ColumnCount, RowCount);
			for (int i = 0; i < mRowCount; i++)
			{
				for (int j = 0; j < mColumnCount; j++)
				{
					mReturnMartix[j, i] = mInnerMatrix[i, j];
				}
			}
			return mReturnMartix;
		}

        public Matrix Inverse()
        {
            if (RowCount != ColumnCount)
            {
                throw new InvalidOperationException();
            }

            Matrix source = new Matrix(this);
            Matrix unity = new Matrix(RowCount, ColumnCount);


            for (int pos = 0; pos < RowCount; pos++)
            {
                unity[pos, pos] = 1;
            }

            /*  */
            for (int col = 0; col < RowCount; col++)
            {
                /* normalize frontmost element */
                if (source[col, col] == 0)
                {
                    throw new InvalidOperationException();
                }

                decimal fact = 1 / source[col, col];
                MultiplyRow(source, col, fact);
                MultiplyRow(unity, col, fact);

                /* set all others to zero */
                for (int zerow = col + 1; zerow < RowCount; zerow++)
                {
                    decimal scale = source[zerow, col];
                    for (int pos = 0; pos < ColumnCount; pos++)
                    {
                        source[zerow, pos] -= source[col, pos] * scale;
                        unity[zerow, pos] -= unity[col, pos] * scale;
                    }
                }
            }

            /* now we have the lower left triangle zeroed and the diagonal zeroed - create identitdy matrix on left side */
            for (int row = 0; row < RowCount; row++)
            {
                for (int col = row + 1; col < ColumnCount; col++)
                {
                    decimal scale = source[row, col];
                    for (int pos = 0; pos < ColumnCount; pos++)
                    {
                        source[row, pos] -= source[col, pos] * scale;
                        unity[row, pos] -= unity[col, pos] * scale;
                    }
                }
            }

            return unity;
        }

        private static void MultiplyRow(Matrix matrix, int row, decimal fact)
        {
            for (int col = 0; col < matrix.ColumnCount; col++)
            {
                matrix[row, col] *= fact;
            }
        }

        public override bool Equals(object obj)
		{
			return base.Equals(obj);
		}
		public override int GetHashCode()
		{
			return base.GetHashCode();
		}

		public bool IsZeroMatrix()
		{
			for (int i = 0; i < this.RowCount; i++)
			{
				for (int j = 0; j < this.ColumnCount; j++)
				{
					if (mInnerMatrix[i, j] != 0)
					{
						return false;
					}
				}
			}
			return true;
		}
		public bool IsSquareMatrix()
		{
			return (this.RowCount == this.ColumnCount);
		}
		public bool IsLowerTriangle()
		{
			if (!this.IsSquareMatrix())
			{
				return false;
			}
			for (int i = 0; i < this.RowCount; i++)
			{
				for (int j = i+1; j<this.ColumnCount; j++)
				{
					if (mInnerMatrix[i, j] != 0)
					{
						return false;
					}
				}
			}
			return true;
		}
		public bool IsUpperTriangle()
		{
			if (!this.IsSquareMatrix())
			{
				return false;
			}
			for (int i = 0; i < this.RowCount; i++)
			{
				for (int j = 0; j < i; j++)
				{
					if (mInnerMatrix[i, j] != 0)
					{
						return false;
					}
				}
			}
			return true;
		}
		public bool IsDiagonalMatrix()
		{
			if (!this.IsSquareMatrix())
			{
				return false;
			}
			for (int i = 0; i < this.RowCount; i++)
			{
				for (int j = 0; j < this.ColumnCount; j++)
				{
					if (i!=j && mInnerMatrix[i, j] != 0)
					{
						return false;
					}
				}
			}
			return true;
		}
		public bool IsIdentityMatrix()
		{
			if (!this.IsSquareMatrix())
			{
				return false;
			}
			for (int i = 0; i < this.RowCount; i++)
			{
				for (int j = 0; j < this.ColumnCount; j++)
				{
					decimal checkValue=0;
					if (i == j)
					{
						checkValue=1;
					}
					if (mInnerMatrix[i, j] != checkValue)
					{
						return false;
					}
				}
			}
			return true;
		}
		public bool IsSymetricMatrix()
		{
			if (!this.IsSquareMatrix())
			{
				return false;
			}
			Matrix transposeMatrix = this.Transpose();
			if (this == transposeMatrix)
			{
				return true;
			}
			else
			{
				return false;
			}
		}


	}
}
